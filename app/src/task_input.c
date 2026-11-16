
/** @file task_input.c
 * @brief Button and RFID input processing task.
 */
#include "task_input.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_cfg.h"
#include "app_ipc.h"
#include "bsp_gpio.h"
#include "hal_rc522.h"
#include "stm32f1xx.h"
#include "task.h"

/** @brief Debounce state of one floor button. */
typedef struct
{
    bool    b_candidate_pressed;
    bool    b_stable_pressed;
    uint8_t stable_samples;
} input_button_state_t;

/** @brief Private state of the RC522 reader. */
typedef struct
{
    bool       b_init_attempted;
    bool       b_ready;
    bool       b_card_latched;
    uint8_t    no_card_samples;
    TickType_t retry_ticks;
    TickType_t last_poll_ticks;
} input_rfid_state_t;

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
static volatile uint32_t g_rfid_successful_reads = 0u;
static volatile uint32_t g_rfid_presentations = 0u;
static volatile uint8_t g_rfid_version = 0u;
static volatile hal_rc522_status_t g_rfid_last_status =
    HAL_RC522_NOT_READY;

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

/** @brief Attempt to initialize the RC522 reader. */
static void input_rfid_initialize(input_rfid_state_t *p_state)
{
    uint8_t version = 0u;

    p_state->b_init_attempted = true;
    p_state->b_ready = false;
    p_state->retry_ticks = xTaskGetTickCount();

    g_rfid_last_status = hal_rc522_init(&version);
    g_rfid_version = version;

    if (HAL_RC522_OK == g_rfid_last_status)
    {
        p_state->b_ready = true;
        p_state->last_poll_ticks = xTaskGetTickCount();
    }
}

/** @brief Schedule reader reinitialization after a communication error. */
static void input_rfid_schedule_retry(
    input_rfid_state_t *p_state,
    hal_rc522_status_t status)
{
    p_state->b_ready = false;
    p_state->no_card_samples = 0u;
    p_state->retry_ticks = xTaskGetTickCount();

    g_rfid_last_status = status;
}

/** @brief Check whether a UID length is supported by the application. */
static bool input_rfid_uid_length_valid(uint8_t uid_length)
{
    return ((4u == uid_length) ||
            (7u == uid_length) ||
            (APP_INPUT_UID_CAPACITY == uid_length));
}

/** @brief Submit AUTH if admission and both epochs remain unchanged. */
static void input_rfid_submit_auth(
    const hal_rc522_uid_t *p_uid,
    const app_input_context_t *p_context_before)
{
    app_input_context_t context_after = {0};
    app_input_event_t event = {0};

    if ((NULL == p_uid) || (NULL == p_context_before))
    {
        input_record_drop();
        return;
    }

    if (pdPASS != input_read_context(&context_after))
    {
        input_record_drop();
        return;
    }

    /*
     * Discard the scan if admission is closed or either epoch
     * changed while the RC522 transaction was in progress.
     */
    if ((!p_context_before->b_admission_open) ||
        (!context_after.b_admission_open) ||
        (p_context_before->authorization_epoch !=
         context_after.authorization_epoch) ||
        (p_context_before->admission_epoch !=
         context_after.admission_epoch))
    {
        input_record_drop();
        return;
    }

    event.kind = APP_INPUT_AUTH;
    event.timestamp_ticks = xTaskGetTickCount();
    event.authorization_epoch = context_after.authorization_epoch;
    event.admission_epoch = context_after.admission_epoch;
    event.b_authorized = true;
    event.uid_length = p_uid->length;

    for (uint8_t uid_index = 0u;
         uid_index < p_uid->length;
         uid_index++)
    {
        event.uid[uid_index] = p_uid->bytes[uid_index];
    }

    (void)input_send_event(&event);
}

/** @brief Process one RC522 poll result. */
static void input_rfid_process_result(
    input_rfid_state_t *p_state,
    hal_rc522_status_t status,
    const hal_rc522_uid_t *p_uid,
    const app_input_context_t *p_context_before)
{
    if (HAL_RC522_NO_CARD == status)
    {
        if (p_state->no_card_samples <
            APP_RFID_NO_CARD_SAMPLES)
        {
            p_state->no_card_samples++;
        }

        if (p_state->no_card_samples >=
            APP_RFID_NO_CARD_SAMPLES)
        {
            p_state->b_card_latched = false;
            p_state->no_card_samples = 0u;
        }

        return;
    }

    if (HAL_RC522_OK != status)
    {
        input_rfid_schedule_retry(p_state, status);
        return;
    }

    g_rfid_successful_reads++;
    p_state->no_card_samples = 0u;

    /* A held card cannot generate another AUTH event. */
    if (p_state->b_card_latched)
    {
        return;
    }

    /* Consume this presentation even if admission is currently closed. */
    p_state->b_card_latched = true;
    g_rfid_presentations++;

    if ((NULL == p_uid) ||
        (!input_rfid_uid_length_valid(p_uid->length)))
    {
        input_rfid_schedule_retry(
            p_state,
            HAL_RC522_PROTOCOL_ERROR);
        return;
    }

    input_rfid_submit_auth(p_uid, p_context_before);
}

/** @brief Poll the RC522 once and process its result. */
static void input_rfid_poll(input_rfid_state_t *p_state)
{
    hal_rc522_uid_t uid = {0};
    app_input_context_t context_before = {0};
    hal_rc522_status_t status;

    /*
     * A failed context read leaves the zero-initialized context
     * with admission closed. The card is still consumed by polling.
     */
    (void)input_read_context(&context_before);

    status = hal_rc522_read_uid(&uid);
    g_rfid_last_status = status;

    input_rfid_process_result(
        p_state,
        status,
        &uid,
        &context_before);
}

/** @brief Service RFID initialization, retry and polling timing. */
static void input_rfid_service(input_rfid_state_t *p_state)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t retry_period = pdMS_TO_TICKS(APP_RFID_RETRY_MS);
    TickType_t poll_period = pdMS_TO_TICKS(APP_RFID_PERIOD_MS);

    if (!p_state->b_init_attempted)
    {
        input_rfid_initialize(p_state);
        return;
    }

    if (!p_state->b_ready)
    {
        if ((TickType_t)(now - p_state->retry_ticks) >= retry_period)
        {
            input_rfid_initialize(p_state);
        }

        return;
    }

    if ((TickType_t)(now - p_state->last_poll_ticks) < poll_period)
    {
        return;
    }

    p_state->last_poll_ticks = now;
    input_rfid_poll(p_state);
}

/** @brief Main entry point of InputTask. */
static void input_task(void *p_argument)
{
    TickType_t last_wake_ticks = xTaskGetTickCount();
    TickType_t input_period = pdMS_TO_TICKS(APP_INPUT_PERIOD_MS);
    input_button_state_t button_states[APP_FLOOR_COUNT] = {0};
    input_rfid_state_t rfid_state = {0};

    (void)p_argument;

    for (;;)
    {
        /* Poll RFID before processing the floor buttons. */
        input_rfid_service(&rfid_state);
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

    return xTaskCreate(
        input_task,
        "InputTask",
        APP_INPUT_STACK_WORDS,
        NULL,
        APP_INPUT_PRIORITY,
        &g_input_task_handle);
}

/*** end of file ***/
