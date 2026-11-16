
/** @file app_ipc.c
 * @brief Startup allocation of the application queues and mailboxes.
 */
#include "app_ipc.h"
#include "app_cfg.h"
#include "task.h"
#include "stm32f1xx.h"

_Static_assert(APP_EMERGENCY_PRIORITY < configMAX_PRIORITIES,
               "Emergency priority requires at least six priority levels");
_Static_assert(1 == INCLUDE_vTaskPrioritySet,
               "MotorTask requires dynamic priority support");
_Static_assert((1u == APP_POSITION_QUEUE_LENGTH) &&
               (1u == APP_DISPLAY_QUEUE_LENGTH),
               "Overwrite mailboxes must have length one");

/* Queue copies are internal to one MCU, never wire-format struct payloads. */
static const UBaseType_t g_queue_lengths[APP_QUEUE_COUNT] =
{
    APP_INPUT_QUEUE_LENGTH,
    APP_FLOOR_QUEUE_LENGTH,
    APP_MOTOR_CMD_QUEUE_LENGTH,
    APP_MOTOR_RPT_QUEUE_LENGTH,
    APP_POSITION_QUEUE_LENGTH,
    APP_DISPLAY_QUEUE_LENGTH
};

static const UBaseType_t g_item_sizes[APP_QUEUE_COUNT] =
{
    sizeof(app_input_event_t),
    sizeof(uint8_t),
    sizeof(app_motor_command_t),
    sizeof(app_motor_report_t),
    sizeof(app_position_t),
    sizeof(app_display_snapshot_t)
};

/* Written before task creation; RTOS queues synchronize subsequent messages. */
static QueueHandle_t gp_queues[APP_QUEUE_COUNT] = {NULL};
static bool gb_initialized = false;

/* ElevatorTask owns updates; critical sections protect coherent copies. */
static app_input_context_t g_input_context = {0};

/** @brief Allocate queues or release partial allocations on failure. */
BaseType_t
app_ipc_init(void)
{
    if ((0u != __get_IPSR()) ||
        (0u != __get_PRIMASK()) ||
        (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }

    if (gb_initialized)
    {
        return pdPASS;
    }

    for (uint32_t queue_index = 0u;
         queue_index < APP_QUEUE_COUNT;
         queue_index++)
    {
        gp_queues[queue_index] = xQueueCreate(
            g_queue_lengths[queue_index],
            g_item_sizes[queue_index]);

        if (NULL == gp_queues[queue_index])
        {
            for (uint32_t cleanup_index = 0u;
                 cleanup_index < queue_index;
                 cleanup_index++)
            {
                vQueueDelete(gp_queues[cleanup_index]);
                gp_queues[cleanup_index] = NULL;
            }

            return pdFAIL;
        }
    }

    gb_initialized = true;

    return pdPASS;
}

/** @brief Publish handles only after complete startup initialization. */
QueueHandle_t
app_ipc_queue(app_queue_id_t queue_id)
{
    /* Negative enum values become large unsigned values and are rejected. */
    uint32_t queue_index = (uint32_t)queue_id;

    if ((!gb_initialized) || (queue_index >= APP_QUEUE_COUNT))
    {
        return NULL;
    }

    return gp_queues[queue_index];
}

/** @brief Publish the latest input context atomically. */
BaseType_t
app_ipc_input_context_publish(
    const app_input_context_t *p_context)
{
    if ((NULL == p_context) ||
        (!gb_initialized) ||
        (0u != __get_IPSR()) ||
        (0u != __get_PRIMASK()) ||
        (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_RUNNING != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }

    taskENTER_CRITICAL();
    g_input_context = *p_context;
    taskEXIT_CRITICAL();

    return pdPASS;
}

/** @brief Read a coherent copy of the latest input context. */
BaseType_t
app_ipc_input_context_read(
    app_input_context_t *p_context)
{
    if ((NULL == p_context) ||
        (!gb_initialized) ||
        (0u != __get_IPSR()) ||
        (0u != __get_PRIMASK()) ||
        (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_RUNNING != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }

    taskENTER_CRITICAL();
    *p_context = g_input_context;
    taskEXIT_CRITICAL();

    return pdPASS;
}

/*** end of file ***/
