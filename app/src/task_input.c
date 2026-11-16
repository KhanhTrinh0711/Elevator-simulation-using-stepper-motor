
/** @file task_input.c
 * @brief Button and RFID input processing task.
 */
#include "task_input.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_cfg.h"
#include "app_ipc.h"
#include "bsp_gpio.h"
#include "task_rfid_test.h"
#include "stm32f1xx.h"
#include "task.h"

/** @brief Debounce state of one floor button. */
typedef struct
{
    bool    b_candidate_pressed;
    bool    b_stable_pressed;
    uint8_t stable_samples;
} input_button_state_t;

/* Floor button pins, ordered from floor 1 to floor 3. */
static const uint16_t g_button_pins[APP_FLOOR_COUNT] =
{
    BUTTON_FLOOR_1_PIN,
    BUTTON_FLOOR_2_PIN,
    BUTTON_FLOOR_3_PIN
};

/* Floor numbers corresponding to g_button_pins. */
static const uint8_t g_button_floors[APP_FLOOR_COUNT] =
{
    1u,
    2u,
    3u
};

/* Private task handle; InputTask is created only once. */
static TaskHandle_t g_input_task_handle = NULL;

/* Diagnostic counters; inspect through the debugger if required. */
static volatile uint32_t g_input_sent_events = 0u;
static volatile uint32_t g_input_dropped_events = 0u;
/** @brief Record one event that could not be delivered. */
static void input_record_drop(void)
{
    g_input_dropped_events++;
}

/** @brief Send one input event without waiting for queue space. */
static BaseType_t input_send_event(
    const app_input_event_t *p_event)
{
    QueueHandle_t input_queue = app_ipc_queue(APP_QUEUE_INPUT);

    if ((NULL == input_queue) || (NULL == p_event))
    {
        input_record_drop();
        return pdFAIL;
    }

    if (pdPASS != xQueueSend(input_queue, p_event, 0u))
    {
        input_record_drop();
        return pdFAIL;
    }

    g_input_sent_events++;

    return pdPASS;
}

/** @brief Read the current admission and session epochs. */
static BaseType_t input_read_context(
    app_input_context_t *p_context)
{
    if (NULL == p_context)
    {
        return pdFAIL;
    }

    return app_ipc_input_context_read(p_context);
}

/** @brief Build and send a debounced floor-button event. */
static void input_submit_floor(uint8_t floor)
{
    app_input_context_t context = {0};
    app_input_event_t event = {0};

    if (pdPASS != input_read_context(&context))
    {
        input_record_drop();
        return;
    }

    if (!context.b_admission_open)
    {
        input_record_drop();
        return;
    }

    event.kind = APP_INPUT_FLOOR;
    event.timestamp_ticks = xTaskGetTickCount();
    event.authorization_epoch = context.authorization_epoch;
    event.admission_epoch = context.admission_epoch;
    event.floor = floor;

    (void)input_send_event(&event);
}

/** @brief Scan buttons and detect stable press transitions. */
static void input_scan_buttons(
    input_button_state_t button_states[APP_FLOOR_COUNT])
{
    for (uint8_t button_index = 0u;
         button_index < APP_FLOOR_COUNT;
         button_index++)
    {
        bool b_sample_pressed =
            (GPIO_PIN_RESET ==
             HAL_GPIO_ReadPin(GPIOA, g_button_pins[button_index]));

        input_button_state_t *p_state =
            &button_states[button_index];

        if (b_sample_pressed != p_state->b_candidate_pressed)
        {
            p_state->b_candidate_pressed = b_sample_pressed;
            p_state->stable_samples = 1u;
        }
        else if (p_state->stable_samples <
                 APP_INPUT_DEBOUNCE_SAMPLES)
        {
            p_state->stable_samples++;
        }

        if ((p_state->stable_samples >=
             APP_INPUT_DEBOUNCE_SAMPLES) &&
            (p_state->b_stable_pressed !=
             p_state->b_candidate_pressed))
        {
            p_state->b_stable_pressed =
                p_state->b_candidate_pressed;

            if (p_state->b_stable_pressed)
            {
                input_submit_floor(g_button_floors[button_index]);
            }
        }
    }
}

/** @brief Main entry point of InputTask. */
static void input_task(void *p_argument)
{
    TickType_t last_wake_ticks = xTaskGetTickCount();
    TickType_t input_period = pdMS_TO_TICKS(APP_INPUT_PERIOD_MS);
    input_button_state_t button_states[APP_FLOOR_COUNT] = {0};

    (void)p_argument;

    for (;;)
    {
        /* Poll RFID before processing the floor buttons. */
        configASSERT(pdPASS == task_rfid_poll());
        input_scan_buttons(button_states);

        /*
         * Prevent rapid catch-up scans if the RC522 transaction
         * caused the task to miss its previous 10 ms wake time.
         */
        if ((TickType_t)(xTaskGetTickCount() - last_wake_ticks) >=
            input_period)
        {
            last_wake_ticks = xTaskGetTickCount();
        }

        vTaskDelayUntil(&last_wake_ticks, input_period);
    }
}

/** @brief Create InputTask before scheduler start. */
BaseType_t
task_input_create(void)
{
    if ((0u != __get_IPSR()) ||
        (0u != __get_PRIMASK()) ||
        (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()) ||
        (NULL != g_input_task_handle) ||
        (NULL == app_ipc_queue(APP_QUEUE_INPUT)))
    {
        return pdFAIL;
    }

    if (pdPASS != task_rfid_configure())
    {
        return pdFAIL;
    }

    return xTaskCreate(
        input_task,
        "InputTask",
        APP_INPUT_STACK_WORDS,
        NULL,
        APP_INPUT_PRIORITY,
        &g_input_task_handle);
}

/*** end of file ***/
