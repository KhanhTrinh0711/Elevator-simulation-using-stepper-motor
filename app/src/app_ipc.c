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
static bool          gb_initialized            = false;

/** @brief Allocate queues or release partial allocations on failure. */
BaseType_t
app_ipc_init (void)
{
    if ((0u != __get_IPSR()) || (0u != __get_PRIMASK()) ||
        (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }

    if (gb_initialized)
    {
        return pdPASS;
    }

    for (uint32_t idx = 0u; idx < APP_QUEUE_COUNT; idx++)
    {
        gp_queues[idx] = xQueueCreate(g_queue_lengths[idx], g_item_sizes[idx]);

        if (NULL == gp_queues[idx])
        {
            for (uint32_t cleanup = 0u; cleanup < idx; cleanup++)
            {
                vQueueDelete(gp_queues[cleanup]);
                gp_queues[cleanup] = NULL;
            }

            return pdFAIL;
        }
    }

    gb_initialized = true;

    return pdPASS;
}

/** @brief Publish handles only after complete startup initialization. */
QueueHandle_t
app_ipc_queue (app_queue_id_t queue_id)
{
    /* Negative enum values become large unsigned values and are rejected. */
    uint32_t idx = (uint32_t)queue_id;

    if ((!gb_initialized) || (idx >= APP_QUEUE_COUNT))
    {
        return NULL;
    }

    return gp_queues[idx];
}

/*** end of file ***/
