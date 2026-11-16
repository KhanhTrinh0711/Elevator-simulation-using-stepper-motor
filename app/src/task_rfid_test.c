/** @file task_rfid_test.c
 * @brief Card presentation filtering for ordered input events.
 */
#include "task_rfid_test.h"
#include "app_cfg.h"
#include "app_ipc.h"
#include "task.h"
#include "stm32f1xx.h"

_Static_assert(APP_INPUT_PRIORITY < configMAX_PRIORITIES,
               "RFID priority exceeds the RTOS range");
_Static_assert(0u < pdMS_TO_TICKS(APP_RFID_PERIOD_MS),
               "RFID polling must yield");

static TaskHandle_t gp_rfid_task = NULL;
static TaskHandle_t gp_owner = NULL;
static QueueHandle_t gh_input = NULL;
static bool gb_configured = false;
static bool gb_ready = false;
static bool gb_armed = true;
static uint8_t g_absent_polls = 0u;
static TickType_t g_last_poll = 0u;
static TickType_t g_wait_ticks = 0u;
/* Shared copies are protected by critical sections, not volatile. */
static task_rfid_snapshot_t g_snapshot = {.status = HAL_RC522_NOT_READY};

static bool rfid_context_valid (void);
static bool rfid_uid_valid (const hal_rc522_uid_t * p_uid);
static void rfid_test_task (void * p_argument);
static void rfid_present (task_rfid_snapshot_t * p_snapshot,
                          const task_rfid_context_t * p_context);

/** @brief Bind the input queue without creating another IPC channel. */
BaseType_t
task_rfid_configure (void)
{
    if ((!rfid_context_valid()) || gb_configured ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    gh_input = app_ipc_queue(APP_QUEUE_INPUT);
    if (NULL == gh_input)
    {
        return pdFAIL;
    }
    g_last_poll = xTaskGetTickCount();
    g_wait_ticks = pdMS_TO_TICKS(TASK_RFID_POWER_WAIT_MS);
    gb_configured = true;
    return pdPASS;
}

/** @brief Admission changes never authorize a card already being held. */
BaseType_t
task_rfid_set_context (const task_rfid_context_t * p_context)
{
    if ((NULL == p_context) || (!rfid_context_valid()) ||
        (!gb_configured) ||
        (taskSCHEDULER_RUNNING != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    const app_input_context_t context = {
        .authorization_epoch = p_context->authorization_epoch,
        .admission_epoch = p_context->admission_epoch,
        .b_admission_open = p_context->b_enabled
    };
    return app_ipc_input_context_publish(&context);
}

/** @brief Standalone adapter, mutually exclusive with InputTask polling. */
BaseType_t
task_rfid_test_create (void)
{
    if ((!rfid_context_valid()) || (!gb_configured) ||
        (NULL != gp_rfid_task) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    return xTaskCreate(rfid_test_task, "RfidInput", APP_INPUT_STACK_WORDS,
                       NULL, APP_INPUT_PRIORITY, &gp_rfid_task);
}

/** @brief Bound retry cadence and consume each presentation only once. */
BaseType_t
task_rfid_poll (void)
{
    if ((!rfid_context_valid()) || (!gb_configured) ||
        (taskSCHEDULER_RUNNING != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    const TaskHandle_t p_caller = xTaskGetCurrentTaskHandle();
    taskENTER_CRITICAL();
    if ((NULL == gp_owner) &&
        ((NULL == gp_rfid_task) || (p_caller == gp_rfid_task)))
    {
        gp_owner = p_caller;
    }
    const bool b_owner = (p_caller == gp_owner);

    task_rfid_snapshot_t snapshot = g_snapshot;
    taskEXIT_CRITICAL();
    if (!b_owner)
    {
        return pdFAIL;
    }

    app_input_context_t shared = {0};
    (void)app_ipc_input_context_read(&shared);
    const task_rfid_context_t context = {
        .authorization_epoch = shared.authorization_epoch,
        .admission_epoch = shared.admission_epoch,
        .b_enabled = shared.b_admission_open
    };
    const TickType_t now = xTaskGetTickCount();
    /* Unsigned tick subtraction intentionally supports a tick rollover. */
    if ((TickType_t)(now - g_last_poll) < g_wait_ticks)
    {
        return pdPASS;
    }
    const hal_rc522_uid_t empty_uid = {0};
    snapshot.uid = empty_uid;
    if (!gb_ready)
    {
        snapshot.status = hal_rc522_init(&snapshot.version);
        gb_ready = (HAL_RC522_OK == snapshot.status);
    }
    if (gb_ready)
    {
        snapshot.status = hal_rc522_read_uid(&snapshot.uid);
    }
    if ((HAL_RC522_OK == snapshot.status) &&
        (!rfid_uid_valid(&snapshot.uid)))
    {
        snapshot.status = HAL_RC522_PROTOCOL_ERROR;
        snapshot.uid = empty_uid;
    }
    snapshot.timestamp_ticks = xTaskGetTickCount();
    g_wait_ticks = pdMS_TO_TICKS(APP_RFID_PERIOD_MS);
    if (HAL_RC522_OK == snapshot.status)
    {
        g_absent_polls = 0u;
        snapshot.successful_reads++;
        if (gb_armed)
        {
            gb_armed = false;
            rfid_present(&snapshot, &context);
        }
    }
    else if (HAL_RC522_NO_CARD == snapshot.status)
    {
        if (TASK_RFID_RELEASE_POLLS > g_absent_polls)
        {
            g_absent_polls++;
        }
        if (TASK_RFID_RELEASE_POLLS == g_absent_polls)
        {
            gb_armed = true;
        }
    }
    else
    {
        g_absent_polls = 0u;
        gb_ready = false;
        g_wait_ticks = pdMS_TO_TICKS(TASK_RFID_RETRY_MS);
    }
    g_last_poll = snapshot.timestamp_ticks;
    taskENTER_CRITICAL();
    g_snapshot = snapshot;
    taskEXIT_CRITICAL();
    return pdPASS;
}

/** @brief Copy diagnostics without exposing mutable shared state. */
BaseType_t
task_rfid_test_snapshot (task_rfid_snapshot_t * p_snapshot)
{
    if ((NULL == p_snapshot) || (!rfid_context_valid()) ||
        (!gb_configured) ||
        (taskSCHEDULER_RUNNING != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    taskENTER_CRITICAL();
    *p_snapshot = g_snapshot;
    taskEXIT_CRITICAL();
    return pdPASS;
}

/** @brief Reject calls which could deadlock RTOS or SPI waiting. */
static bool
rfid_context_valid (void)
{
    return ((0u == __get_IPSR()) && (0u == __get_PRIMASK()) &&
            (0u == __get_BASEPRI()));
}

/** @brief Only complete ISO14443A UIDs are identity keys. */
static bool
rfid_uid_valid (const hal_rc522_uid_t * p_uid)
{
    return ((4u == p_uid->length) || (7u == p_uid->length) ||
            (10u == p_uid->length));
}

/** @brief Never defer an event until after a lock or a full queue clears. */
static void
rfid_present (task_rfid_snapshot_t * p_snapshot,
               const task_rfid_context_t * p_context)
{
    app_input_event_t event = {0};
    event.kind = APP_INPUT_AUTH;
    event.timestamp_ticks = p_snapshot->timestamp_ticks;
    event.authorization_epoch = p_context->authorization_epoch;
    event.admission_epoch = p_context->admission_epoch;
    /* shortcut: accept any UID, add an allowlist when access rules are set. */
    event.b_authorized = true;
    event.uid_length = p_snapshot->uid.length;
    for (uint32_t idx = 0u; idx < event.uid_length; idx++)
    {
        event.uid[idx] = p_snapshot->uid.bytes[idx];
    }
    p_snapshot->presentations++;

    /* Elevator also rejects a latched Emergency and any later epoch change. */
    app_input_context_t current = {0};
    (void)app_ipc_input_context_read(&current);
    /* Receiver rejects any epoch changed after this coherent copy. */
    const bool b_current = (p_context->b_enabled &&
        current.b_admission_open &&
        (p_context->admission_epoch == current.admission_epoch) &&
        (p_context->authorization_epoch == current.authorization_epoch));
    if (b_current && (pdPASS == xQueueSend(gh_input, &event, 0u)))
    {
        p_snapshot->sent_events++;
    }
    else
    {
        p_snapshot->dropped_events++;
    }
}

/** @brief Adapter only; production InputTask calls the same poll function. */
static void
rfid_test_task (void * p_argument)
{
    /* Required RTOS entry signature; no caller-owned argument is retained. */
    (void)p_argument;
    for (;;)
    {
        configASSERT(pdPASS == task_rfid_poll());
        vTaskDelay(pdMS_TO_TICKS(APP_INPUT_PERIOD_MS));
    }
}

/*** end of file ***/
