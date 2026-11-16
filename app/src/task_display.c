/** @file task_display.c
 * @brief Render elevator snapshots without owning FSM or access decisions.
 */
#include "task_display.h"
#include <stdbool.h>
#include "app_cfg.h"
#include "app_ipc.h"
#include "bsp_gpio.h"
#include "hal_oled.h"
#include "task.h"

_Static_assert(APP_DISPLAY_PRIORITY < configMAX_PRIORITIES,
               "Display priority exceeds RTOS range");
_Static_assert(0u < pdMS_TO_TICKS(TASK_DISPLAY_POLL_MS),
               "Display polling must yield");
_Static_assert(0u < pdMS_TO_TICKS(TASK_DISPLAY_RED_TOGGLE_MS),
               "LED half-period must be nonzero");
_Static_assert(3000u == APP_AUTH_WINDOW_MS,
               "RFID text format assumes the agreed three-second window");

static TaskHandle_t gp_display_task = NULL;
static QueueHandle_t gh_display_queue = NULL;

static void display_task (void * p_argument);
static bool display_valid (const app_display_snapshot_t * p_snapshot);
static bool display_ready (const app_display_snapshot_t * p_snapshot);
static const char * display_state (app_state_t state);
static HAL_StatusTypeDef display_draw (
    const app_display_snapshot_t * p_snapshot, TickType_t now, bool b_fresh);

/** @brief Bind the mailbox and initialize only the two display-owned LEDs. */
BaseType_t
task_display_create (void)
{
    if ((0u != __get_IPSR()) || (0u != __get_PRIMASK()) ||
        (0u != __get_BASEPRI()) || (NULL != gp_display_task) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    gh_display_queue = app_ipc_queue(APP_QUEUE_DISPLAY);
    if (NULL == gh_display_queue)
    {
        return pdFAIL;
    }
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN | LED_RED_PIN, GPIO_PIN_RESET);
    GPIO_InitTypeDef pins = {0};
    pins.Pin = LED_GREEN_PIN | LED_RED_PIN;
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Pull = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &pins);
    return xTaskCreate(display_task, "Display", APP_DISPLAY_STACK_WORDS,
                       NULL, APP_DISPLAY_PRIORITY, &gp_display_task);
}

/** @brief Service LEDs and mailbox even while OLED recovery is pending. */
static void
display_task (void * p_argument)
{
    /* FreeRTOS requires this signature; there is no caller-owned argument. */
    (void)p_argument;
    app_display_snapshot_t snapshot = {0};
    bool b_have_data = false;
    bool b_oled_ready = false;
    bool b_emergency = false;
    TickType_t red_start = 0u;
    vTaskDelay(pdMS_TO_TICKS(TASK_DISPLAY_POWER_WAIT_MS));
    TickType_t wake = xTaskGetTickCount();
    TickType_t last_draw = wake;
    TickType_t draw_wait = 0u;

    for (;;)
    {
        app_display_snapshot_t incoming = {0};
        const TickType_t now = xTaskGetTickCount();
        if (pdPASS == xQueueReceive(gh_display_queue, &incoming, 0u))
        {
            b_have_data = display_valid(&incoming);
            if (b_have_data)
            {
                snapshot = incoming;
            }
        }
        /* Tick subtraction is deliberately modulo TickType_t. */
        const bool b_fresh = (b_have_data &&
            ((TickType_t)(now - snapshot.timestamp_ticks) <
             pdMS_TO_TICKS(TASK_DISPLAY_STALE_MS)));
        if (b_fresh)
        {
            const bool b_new = (APP_STATE_EMERGENCY == snapshot.state);
            if (b_new && (!b_emergency))
            {
                red_start = now;
            }
            b_emergency = b_new;
        }
        /* Keep the last known Emergency indication if the publisher stops. */
        const bool b_red = (b_emergency &&
            (0u == (((TickType_t)(now - red_start) /
                     pdMS_TO_TICKS(TASK_DISPLAY_RED_TOGGLE_MS)) % 2u)));
        HAL_GPIO_WritePin(GPIOB, LED_RED_PIN,
                          b_red ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN,
            (b_fresh && display_ready(&snapshot)) ?
             GPIO_PIN_SET : GPIO_PIN_RESET);

        if ((TickType_t)(now - last_draw) >= draw_wait)
        {
            HAL_StatusTypeDef status = HAL_OK;
            if (!b_oled_ready)
            {
                status = hal_oled_init();
            }
            if (HAL_OK == status)
            {
                status = display_draw(&snapshot, xTaskGetTickCount(),
                                      b_fresh);
            }
            b_oled_ready = (HAL_OK == status);
            last_draw = xTaskGetTickCount();
            draw_wait = pdMS_TO_TICKS(b_oled_ready ? APP_DISPLAY_PERIOD_MS :
                                     TASK_DISPLAY_RETRY_MS);
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(TASK_DISPLAY_POLL_MS));
    }
}

/** @brief Do not index state tables or render impossible input as valid. */
static bool
display_valid (const app_display_snapshot_t * p_snapshot)
{
    return ((NULL != display_state(p_snapshot->state)) &&
        ((APP_DIRECTION_STOPPED == p_snapshot->direction) ||
         (APP_DIRECTION_UP == p_snapshot->direction) ||
         (APP_DIRECTION_DOWN == p_snapshot->direction)) &&
        (APP_FLOOR_COUNT >= p_snapshot->current_floor) &&
        (APP_FLOOR_QUEUE_LENGTH >= p_snapshot->waiting_count) &&
        (APP_AUTH_WINDOW_MS >= p_snapshot->auth_remaining_ms));
}

/** @brief Producer certifies no target/lock; also check visible invariants. */
static bool
display_ready (const app_display_snapshot_t * p_snapshot)
{
    return (p_snapshot->b_ready && (APP_STATE_IDLE == p_snapshot->state) &&
        (APP_DIRECTION_STOPPED == p_snapshot->direction) &&
        (0u != p_snapshot->current_floor) &&
        (0u == p_snapshot->waiting_count));
}

/** @brief Match the six states in the shared FSM contract. */
static const char *
display_state (app_state_t state)
{
    switch (state)
    {
        case APP_STATE_INIT:
            return "STATE: INIT";
        case APP_STATE_HOMING:
            return "STATE: HOMING";
        case APP_STATE_IDLE:
            return "STATE: IDLE";
        case APP_STATE_MOVING:
            return "STATE: MOVING";
        case APP_STATE_ARRIVED:
            return "STATE: ARRIVED";
        case APP_STATE_EMERGENCY:
            return "STATE: EMERGENCY";
        default:
            return NULL;
    }
}

/** @brief Small fixed-width fields avoid printf and a full framebuffer. */
static HAL_StatusTypeDef
display_draw (const app_display_snapshot_t * p_snapshot,
               TickType_t now, bool b_fresh)
{
    /* Recheck age after potentially slow OLED initialization. */
    const TickType_t age = now - p_snapshot->timestamp_ticks;
    b_fresh = (b_fresh && (age < pdMS_TO_TICKS(TASK_DISPLAY_STALE_MS)));
    char floor[] = "FLOOR: ?";
    char waiting[] = "QUEUE: ?";
    char auth[] = "RFID: 0.0 S";
    const char * p_direction = "DIR: ?";
    if (b_fresh)
    {
        /* All values are validated single decimal digits before narrowing. */
        if (0u != p_snapshot->current_floor)
        {
            floor[7] = (char)('0' + p_snapshot->current_floor);
        }
        waiting[7] = (char)('0' + p_snapshot->waiting_count);
        p_direction = (APP_DIRECTION_UP == p_snapshot->direction) ?
                       "DIR: UP" : "DIR: DOWN";
        if (APP_DIRECTION_STOPPED == p_snapshot->direction)
        {
            p_direction = "DIR: STOPPED";
        }
        const uint32_t age_ms = age * portTICK_PERIOD_MS;
        const uint32_t remaining =
            (p_snapshot->auth_remaining_ms > age_ms) ?
             (p_snapshot->auth_remaining_ms - age_ms) : 0u;
        if ((APP_STATE_IDLE == p_snapshot->state) ||
            (APP_STATE_MOVING == p_snapshot->state) ||
            (APP_STATE_ARRIVED == p_snapshot->state))
        {
            /* Ceiling to tenths: 1..100 ms displays 0.1, never early 0.0. */
            const uint32_t tenths = (remaining + 99u) / 100u;
            auth[6] = (char)('0' + (tenths / 10u));
            auth[8] = (char)('0' + (tenths % 10u));
        }
    }
    const char * const lines[] =
    {
        b_fresh ? display_state(p_snapshot->state) : "STATE: NO DATA",
        floor, p_direction, waiting, auth,
        (b_fresh && display_ready(p_snapshot)) ? "READY: YES" : "READY: NO"
    };
    for (uint32_t row = 0u; row < (sizeof(lines) / sizeof(lines[0])); row++)
    {
        /* Six rows fit the OLED's 8-bit page argument. */
        const HAL_StatusTypeDef status = hal_oled_write_line(
            (uint8_t)row, lines[row]);
        if (HAL_OK != status)
        {
            return status;
        }
    }
    return HAL_OK;
}

/*** end of file ***/
