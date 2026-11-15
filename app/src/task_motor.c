/**
 * @file task_motor.c
 * @brief MotorTask command execution and AS5600 position monitoring.
 *
 * MotorTask is the sole application-level owner of the stepper driver and
 * AS5600 position update API. I2C serialization is provided by the BSP.
 */

#include "task_motor.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_as5600.h"
#include "hal_stepper.h"
#include "queue.h"
#include "task.h"

#if (APP_POSITION_MAX_GAP_MS < APP_POSITION_PERIOD_MS)
#error "Encoder maximum sample gap must be >= sample period"
#endif

#if (APP_MOTOR_HOME_TIMEOUT_MS == 0u)
#error "APP_MOTOR_HOME_TIMEOUT_MS must be non-zero"
#endif

#if (APP_MOTOR_HOME_STABLE_SAMPLES == 0u)
#error "APP_MOTOR_HOME_STABLE_SAMPLES must be non-zero"
#endif

#if (APP_MOTOR_MECHANICS_CALIBRATED == 1u)
#if (APP_MOTOR_STEPS_PER_ENCODER_REV == 0u)
#error "Configure measured stepper steps per encoder revolution"
#endif
#if (APP_MOTOR_MOVE_TIMEOUT_MS == 0u)
#error "Configure a non-zero motor move timeout"
#endif
#if (APP_MOTOR_POSITIVE_DIR_CW > 1u)
#error "Configure the measured positive encoder direction"
#endif
#endif

/**
 * @brief Private MotorTask runtime data.
 */
typedef struct
{
    QueueHandle_t command_queue;
    QueueHandle_t report_queue;
    QueueHandle_t position_queue;

    bool b_sensor_initialized;
    bool b_magnet_detected;
    bool b_sample_valid;
    bool b_reference_valid;
    bool b_have_sample;

    TickType_t last_sample_ticks;
    TickType_t last_magnet_check_ticks;
    TickType_t last_init_attempt_ticks;

    bool b_motion_active;
    uint32_t motion_command_id;
    int32_t motion_start_counts;
    int32_t motion_target_counts;
    TickType_t motion_start_ticks;

    bool b_home_pending;
    uint32_t home_command_id;
    uint32_t home_stable_samples;
    int32_t home_previous_counts;
    TickType_t home_start_ticks;

    bool b_emergency_seen;

    bool b_report_pending;
    app_motor_report_t pending_report;
} task_motor_context_t;

/* Task handle is retained to prevent duplicate task creation. */
static TaskHandle_t g_motor_task_handle = NULL;

/* Static storage is zero-initialized before the scheduler starts. */
static task_motor_context_t g_motor_context;

/* Private function declarations. */
static void task_motor_entry(void *argument);
static void task_motor_try_sensor_init(task_motor_context_t *context,
                                       TickType_t now);
static bool task_motor_sample_encoder(task_motor_context_t *context,
                                      TickType_t now);
static void task_motor_publish_position(task_motor_context_t *context,
                                        TickType_t now,
                                        bool b_sample_ok);
static void task_motor_set_priority(bool b_active);
static void task_motor_record_report(task_motor_context_t *context,
                                     app_motor_result_t result,
                                     uint32_t command_id);
static void task_motor_flush_report(task_motor_context_t *context);
static void task_motor_handle_emergency(task_motor_context_t *context);
static void task_motor_receive_command(task_motor_context_t *context,
                                       TickType_t now,
                                       bool b_sample_ok);
static void task_motor_process_command(task_motor_context_t *context,
                                       const app_motor_command_t *command,
                                       TickType_t now,
                                       bool b_sample_ok);
static void task_motor_start_move(task_motor_context_t *context,
                                  const app_motor_command_t *command,
                                  TickType_t now,
                                  bool b_sample_ok);
static void task_motor_start_home(task_motor_context_t *context,
                                  const app_motor_command_t *command,
                                  TickType_t now,
                                  bool b_sample_ok);
static void task_motor_update_move(task_motor_context_t *context,
                                   TickType_t now,
                                   bool b_sample_ok);
static void task_motor_update_home(task_motor_context_t *context,
                                   TickType_t now,
                                   bool b_sample_ok);
static bool task_motor_target_reached(const task_motor_context_t *context,
                                      int32_t current_counts);

/**
 * @brief Create MotorTask.
 *
 * The IPC queues must have been created before this function is called.
 *
 * @return pdPASS when the task is created; pdFAIL otherwise.
 */
BaseType_t task_motor_create(void)
{
    BaseType_t result;

    if (NULL != g_motor_task_handle)
    {
        return pdFAIL;
    }

    g_motor_context.command_queue =
        app_ipc_queue(APP_QUEUE_MOTOR_COMMAND);
    g_motor_context.report_queue =
        app_ipc_queue(APP_QUEUE_MOTOR_REPORT);
    g_motor_context.position_queue =
        app_ipc_queue(APP_QUEUE_POSITION);

    if ((NULL == g_motor_context.command_queue) ||
        (NULL == g_motor_context.report_queue) ||
        (NULL == g_motor_context.position_queue))
    {
        return pdFAIL;
    }

    result = xTaskCreate(task_motor_entry,
                         "MOTOR_TASK",
                         APP_MOTOR_STACK_WORDS,
                         NULL,
                         APP_MOTOR_PRIORITY_IDLE,
                         &g_motor_task_handle);

    if (pdPASS != result)
    {
        g_motor_task_handle = NULL;
    }

    return result;
}

/**
 * @brief MotorTask entry point.
 *
 * Samples the encoder periodically, publishes the latest position and
 * handles commands. Emergency hardware shutdown is performed by the
 * emergency ISR; this task handles software cancellation and reporting.
 *
 * @param argument Unused task argument.
 */
static void task_motor_entry(void *argument)
{
    TickType_t last_wake_time;
    TickType_t now;
    bool b_sample_ok;

    (void)argument;

    last_wake_time = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(APP_POSITION_PERIOD_MS));

        now = xTaskGetTickCount();

        if (false == g_motor_context.b_sensor_initialized)
        {
            task_motor_try_sensor_init(&g_motor_context, now);
        }

        b_sample_ok =
            task_motor_sample_encoder(&g_motor_context, now);

        task_motor_publish_position(&g_motor_context,
                                    now,
                                    b_sample_ok);

        task_motor_flush_report(&g_motor_context);

        if (0u != Stepper_IsEmergencyLatched())
        {
            task_motor_handle_emergency(&g_motor_context);

            /*
             * During Emergency, only HOME is accepted. HOME is a
             * stationary reference confirmation and never starts motion.
             */
            if (g_motor_context.b_home_pending)
            {
                task_motor_update_home(&g_motor_context,
                                       now,
                                       b_sample_ok);
            }
            else if (false == g_motor_context.b_report_pending)
            {
                task_motor_receive_command(&g_motor_context,
                                           now,
                                           b_sample_ok);
            }
        }
        else
        {
            g_motor_context.b_emergency_seen = false;

            if (g_motor_context.b_home_pending)
            {
                /* A reference-confirmation operation must remain in
                 * Emergency. Cancel it if Emergency was released early.
                 */
                g_motor_context.b_home_pending = false;
                task_motor_set_priority(false);
                task_motor_record_report(&g_motor_context,
                                         APP_MOTOR_ERROR,
                                         g_motor_context.home_command_id);
            }

            if (g_motor_context.b_motion_active)
            {
                task_motor_update_move(&g_motor_context,
                                       now,
                                       b_sample_ok);
            }

            if (false == g_motor_context.b_report_pending)
            {
                task_motor_receive_command(&g_motor_context,
                                           now,
                                           b_sample_ok);
            }
        }

        task_motor_flush_report(&g_motor_context);
    }
}

/**
 * @brief Attempt to initialize AS5600 from task context.
 *
 * Initialization establishes an angle-tracking baseline only. It does not
 * establish the elevator's floor reference.
 *
 * @param context MotorTask runtime data.
 * @param now Current RTOS tick.
 */
static void task_motor_try_sensor_init(task_motor_context_t *context,
                                      TickType_t now)
{
    HAL_StatusTypeDef status;
    uint8_t magnet_detected;

    if ((TickType_t)(now - context->last_init_attempt_ticks) <
        pdMS_TO_TICKS(APP_RFID_PERIOD_MS))
    {
        return;
    }

    context->last_init_attempt_ticks = now;

    status = HAL_AS5600_Init();

    if (HAL_OK != status)
    {
        return;
    }

    context->b_sensor_initialized = true;
    context->b_have_sample = false;
    context->b_sample_valid = false;
    context->b_reference_valid = false;

    status = HAL_AS5600_IsMagnetDetected(&magnet_detected);

    if ((HAL_OK == status) && (0u != magnet_detected))
    {
        context->b_magnet_detected = true;
    }
    else
    {
        context->b_magnet_detected = false;
    }

    context->last_sample_ticks = now;
    context->last_magnet_check_ticks = now;
}

/**
 * @brief Read encoder and validate sample continuity.
 *
 * If the interval between successful samples is too long, the multi-turn
 * count is no longer trusted. A later stationary HOME operation is required
 * to establish a new reference.
 *
 * @param context MotorTask runtime data.
 * @param now Current RTOS tick.
 * @return true when this sample is valid.
 */
static bool task_motor_sample_encoder(task_motor_context_t *context,
                                      TickType_t now)
{
    HAL_StatusTypeDef status;
    uint8_t magnet_detected;
    bool b_interval_valid;

    if (false == context->b_sensor_initialized)
    {
        context->b_sample_valid = false;
        return false;
    }

    status = HAL_AS5600_UpdatePosition();

    if (HAL_OK != status)
    {
        context->b_sample_valid = false;
        context->b_reference_valid = false;
        return false;
    }

    b_interval_valid = true;

    if (context->b_have_sample)
    {
        if ((TickType_t)(now - context->last_sample_ticks) >
            pdMS_TO_TICKS(APP_POSITION_MAX_GAP_MS))
        {
            b_interval_valid = false;
        }
    }

    context->last_sample_ticks = now;
    context->b_have_sample = true;

    if ((TickType_t)(now - context->last_magnet_check_ticks) >=
        pdMS_TO_TICKS(APP_MOTOR_MAGNET_CHECK_PERIOD_MS))
    {
        status = HAL_AS5600_IsMagnetDetected(&magnet_detected);
        context->last_magnet_check_ticks = now;

        if ((HAL_OK == status) && (0u != magnet_detected))
        {
            context->b_magnet_detected = true;
        }
        else
        {
            context->b_magnet_detected = false;
        }
    }

    context->b_sample_valid =
        b_interval_valid && context->b_magnet_detected;

    if (false == context->b_sample_valid)
    {
        context->b_reference_valid = false;
    }

    return context->b_sample_valid;
}

/**
 * @brief Publish the newest encoder sample.
 *
 * PositionQueue is a one-element mailbox, so the newest sample replaces
 * the previous one.
 *
 * @param context MotorTask runtime data.
 * @param now Current RTOS tick.
 * @param b_sample_ok Current sample validity.
 */
static void task_motor_publish_position(task_motor_context_t *context,
                                        TickType_t now,
                                        bool b_sample_ok)
{
    app_position_t position;

    if (context->b_sensor_initialized)
    {
        position.counts = HAL_AS5600_GetPosition();
    }
    else
    {
        position.counts = 0;
    }

    position.timestamp_ticks = now;

    /*
     * A valid sensor sample does not imply that the floor reference
     * has been calibrated. HOME establishes the reference-valid state.
     */
    position.b_valid =
        b_sample_ok &&
        context->b_sample_valid &&
        context->b_reference_valid;

    if (pdPASS != xQueueOverwrite(context->position_queue, &position))
    {
        context->b_sample_valid = false;
    }
}

/**
 * @brief Set MotorTask priority according to its activity.
 *
 * @param b_active true while verifying HOME or executing MOVE.
 */
static void task_motor_set_priority(bool b_active)
{
    if (b_active)
    {
        vTaskPrioritySet(NULL, APP_MOTOR_PRIORITY_ACTIVE);
    }
    else
    {
        vTaskPrioritySet(NULL, APP_MOTOR_PRIORITY_IDLE);
    }
}

/**
 * @brief Save one terminal report for later queue delivery.
 *
 * @param context MotorTask runtime data.
 * @param result Terminal motor result.
 * @param command_id Related command identifier.
 */
static void task_motor_record_report(task_motor_context_t *context,
                                     app_motor_result_t result,
                                     uint32_t command_id)
{
    if (false == context->b_report_pending)
    {
        context->pending_report.result = result;
        context->pending_report.command_id = command_id;

        context->pending_report.position.counts = 0;
        context->pending_report.position.timestamp_ticks =
            xTaskGetTickCount();
        context->pending_report.position.b_valid = false;

        if (context->b_sensor_initialized)
        {
            context->pending_report.position.counts =
                HAL_AS5600_GetPosition();
            context->pending_report.position.b_valid =
                context->b_sample_valid &&
                context->b_reference_valid;
        }

        context->b_report_pending = true;
    }
}

/**
 * @brief Send a pending terminal report when queue space is available.
 *
 * @param context MotorTask runtime data.
 */
static void task_motor_flush_report(task_motor_context_t *context)
{
    if (context->b_report_pending)
    {
        if (pdPASS == xQueueSend(context->report_queue,
                                 &context->pending_report,
                                 0u))
        {
            context->b_report_pending = false;
        }
    }
}

/**
 * @brief Handle the first MotorTask iteration after Emergency is latched.
 *
 * Commands already queued before Emergency are discarded. The active
 * command receives a STOPPED report. The ISR has already switched off
 * the stepper outputs.
 *
 * @param context MotorTask runtime data.
 */
static void task_motor_handle_emergency(task_motor_context_t *context)
{
    if (false == context->b_emergency_seen)
    {
        context->b_emergency_seen = true;

        (void)xQueueReset(context->command_queue);

        if (context->b_motion_active)
        {
            context->b_motion_active = false;
            task_motor_set_priority(false);
            task_motor_record_report(context,
                                     APP_MOTOR_STOPPED,
                                     context->motion_command_id);
        }
        else if (context->b_home_pending)
        {
            context->b_home_pending = false;
            task_motor_set_priority(false);
            task_motor_record_report(context,
                                     APP_MOTOR_STOPPED,
                                     context->home_command_id);
        }
        else
        {
            task_motor_set_priority(false);
        }
    }
}

/**
 * @brief Receive and dispatch one command.
 *
 * While Emergency is latched, only HOME is accepted. Any other command
 * is rejected and reported as STOPPED.
 *
 * @param context MotorTask runtime data.
 * @param now Current RTOS tick.
 * @param b_sample_ok Current encoder sample validity.
 */
static void task_motor_receive_command(task_motor_context_t *context,
                                       TickType_t now,
                                       bool b_sample_ok)
{
    app_motor_command_t command;

    if (pdPASS == xQueueReceive(context->command_queue, &command, 0u))
    {
        if (0u != Stepper_IsEmergencyLatched())
        {
            if (APP_MOTOR_HOME == command.action)
            {
                task_motor_start_home(context,
                                      &command,
                                      now,
                                      b_sample_ok);
            }
            else
            {
                task_motor_record_report(context,
                                         APP_MOTOR_STOPPED,
                                         command.command_id);
            }
        }
        else
        {
            task_motor_process_command(context,
                                       &command,
                                       now,
                                       b_sample_ok);
        }
    }
}

/**
 * @brief Dispatch a motor command during normal operation.
 *
 * @param context MotorTask runtime data.
 * @param command Received command.
 * @param now Current RTOS tick.
 * @param b_sample_ok Current encoder sample validity.
 */
static void task_motor_process_command(task_motor_context_t *context,
                                       const app_motor_command_t *command,
                                       TickType_t now,
                                       bool b_sample_ok)
{
    if (APP_MOTOR_STOP == command->action)
    {
        if (context->b_motion_active)
        {
            Stepper_Stop();
            context->b_motion_active = false;
            task_motor_set_priority(false);
            task_motor_record_report(context,
                                     APP_MOTOR_STOPPED,
                                     context->motion_command_id);
        }
        else
        {
            task_motor_record_report(context,
                                     APP_MOTOR_STOPPED,
                                     command->command_id);
        }
    }
    else if (APP_MOTOR_MOVE == command->action)
    {
        if (context->b_motion_active || context->b_home_pending)
        {
            task_motor_record_report(context,
                                     APP_MOTOR_ERROR,
                                     command->command_id);
        }
        else
        {
            task_motor_start_move(context,
                                  command,
                                  now,
                                  b_sample_ok);
        }
    }
    else if (APP_MOTOR_HOME == command->action)
    {
        /*
         * HOME is deliberately rejected outside Emergency. This prevents
         * an ordinary application command from changing the reference.
         */
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 command->command_id);
    }
    else
    {
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 command->command_id);
    }
}

/**
 * @brief Start a MOVE command when mechanical calibration is available.
 *
 * The target must be one of the three configured floor positions:
 * 0, 4096 or 8192 encoder counts relative to the confirmed floor-1 zero.
 *
 * @param context MotorTask runtime data.
 * @param command MOVE command.
 * @param now Current RTOS tick.
 * @param b_sample_ok Current encoder sample validity.
 */
static void task_motor_start_move(task_motor_context_t *context,
                                  const app_motor_command_t *command,
                                  TickType_t now,
                                  bool b_sample_ok)
{
    if ((false == context->b_sensor_initialized) ||
        (false == context->b_magnet_detected) ||
        (false == context->b_sample_valid) ||
        (false == context->b_reference_valid) ||
        (false == b_sample_ok))
    {
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 command->command_id);
        return;
    }

    if (command->target_counts < 0)
    {
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 command->command_id);
        return;
    }

    if (((uint32_t)command->target_counts >
         ((APP_FLOOR_COUNT - 1u) * HAL_AS5600_COUNTS_PER_REV)) ||
        (0 != (command->target_counts %
               HAL_AS5600_COUNTS_PER_REV)))
    {
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 command->command_id);
        return;
    }

#if (APP_MOTOR_MECHANICS_CALIBRATED == 1u)
    {
        int32_t current_counts;
        int64_t delta_counts;
        uint64_t distance_counts;
        uint64_t steps_wide;
        uint32_t steps;
        Stepper_Direction_t direction;
        HAL_StatusTypeDef status;

        current_counts = HAL_AS5600_GetPosition();

        delta_counts = (int64_t)command->target_counts -
                       (int64_t)current_counts;

        if (0 == delta_counts)
        {
            task_motor_record_report(context,
                                     APP_MOTOR_ARRIVED,
                                     command->command_id);
            return;
        }

        if (delta_counts > 0)
        {
            distance_counts = (uint64_t)delta_counts;
#if (APP_MOTOR_POSITIVE_DIR_CW == 1u)
            direction = STEPPER_DIR_CW;
#else
            direction = STEPPER_DIR_CCW;
#endif
        }
        else
        {
            distance_counts = (uint64_t)(-delta_counts);
#if (APP_MOTOR_POSITIVE_DIR_CW == 1u)
            direction = STEPPER_DIR_CCW;
#else
            direction = STEPPER_DIR_CW;
#endif
        }

        steps_wide =
            ((distance_counts *
              (uint64_t)APP_MOTOR_STEPS_PER_ENCODER_REV) +
             (HAL_AS5600_COUNTS_PER_REV / 2u)) /
            HAL_AS5600_COUNTS_PER_REV;

        if ((0u == steps_wide) || (steps_wide > UINT32_MAX))
        {
            task_motor_record_report(context,
                                     APP_MOTOR_ERROR,
                                     command->command_id);
            return;
        }

        /* The preceding range check makes this conversion safe. */
        steps = (uint32_t)steps_wide;

        status = Stepper_Start(steps, direction);

        if (HAL_OK != status)
        {
            task_motor_record_report(context,
                                     APP_MOTOR_ERROR,
                                     command->command_id);
            return;
        }

        context->b_motion_active = true;
        context->motion_command_id = command->command_id;
        context->motion_start_counts = current_counts;
        context->motion_target_counts = command->target_counts;
        context->motion_start_ticks = now;

        task_motor_set_priority(true);
    }
#else
    (void)now;

    /*
     * Do not guess the mechanical ratio or direction. MOVE remains
     * disabled until the measured values are configured in app_cfg.h.
     */
    task_motor_record_report(context,
                             APP_MOTOR_ERROR,
                             command->command_id);
#endif
}

/**
 * @brief Start stationary HOME verification during Emergency.
 *
 * ElevatorTask must send HOME only after the operator procedure and
 * mechanical reference-switch conditions have been verified. This
 * function never starts the stepper.
 *
 * @param context MotorTask runtime data.
 * @param command HOME command.
 * @param now Current RTOS tick.
 * @param b_sample_ok Current encoder sample validity.
 */
static void task_motor_start_home(task_motor_context_t *context,
                                  const app_motor_command_t *command,
                                  TickType_t now,
                                  bool b_sample_ok)
{
    if ((0u == Stepper_IsEmergencyLatched()) ||
        (false == context->b_sensor_initialized) ||
        (false == context->b_magnet_detected) ||
        (false == b_sample_ok) ||
        (0u != Stepper_IsBusy()))
    {
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 command->command_id);
        return;
    }

    context->b_home_pending = true;
    context->home_command_id = command->command_id;
    context->home_stable_samples = 1u;
    context->home_previous_counts = HAL_AS5600_GetPosition();
    context->home_start_ticks = now;

    task_motor_set_priority(true);
}

/**
 * @brief Check whether the encoder has reached or passed the target.
 *
 * @param context MotorTask runtime data.
 * @param current_counts Current encoder count.
 * @return true when target is reached or passed in the travel direction.
 */
static bool task_motor_target_reached(const task_motor_context_t *context,
                                      int32_t current_counts)
{
    int64_t target_counts;
    int64_t current_position;
    int64_t start_counts;

    target_counts = (int64_t)context->motion_target_counts;
    current_position = (int64_t)current_counts;
    start_counts = (int64_t)context->motion_start_counts;

    if (target_counts >= start_counts)
    {
        return (current_position >= target_counts);
    }

    return (current_position <= target_counts);
}

/**
 * @brief Monitor an active movement command.
 *
 * ARRIVED is reported only when the encoder confirms the target. Finishing
 * the programmed step count alone is not sufficient.
 *
 * @param context MotorTask runtime data.
 * @param now Current RTOS tick.
 * @param b_sample_ok Current encoder sample validity.
 */
static void task_motor_update_move(task_motor_context_t *context,
                                   TickType_t now,
                                   bool b_sample_ok)
{
    int32_t current_counts;

    current_counts = HAL_AS5600_GetPosition();

    if ((false == b_sample_ok) ||
        (false == context->b_sample_valid))
    {
        Stepper_Stop();
        context->b_motion_active = false;
        task_motor_set_priority(false);
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 context->motion_command_id);
    }
    else if (task_motor_target_reached(context, current_counts))
    {
        Stepper_Stop();
        context->b_motion_active = false;
        task_motor_set_priority(false);
        task_motor_record_report(context,
                                 APP_MOTOR_ARRIVED,
                                 context->motion_command_id);
    }
    else if ((TickType_t)(now - context->motion_start_ticks) >=
             pdMS_TO_TICKS(APP_MOTOR_MOVE_TIMEOUT_MS))
    {
        Stepper_Stop();
        context->b_motion_active = false;
        task_motor_set_priority(false);
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 context->motion_command_id);
    }
    else if (0u == Stepper_IsBusy())
    {
        context->b_motion_active = false;
        task_motor_set_priority(false);
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 context->motion_command_id);
    }
}

/**
 * @brief Verify a stationary reference while Emergency remains latched.
 *
 * After the required number of stable samples, the encoder count is set
 * to zero. The mechanical switch validation is the responsibility of
 * ElevatorTask/operator procedure and must occur before HOME is sent.
 *
 * @param context MotorTask runtime data.
 * @param now Current RTOS tick.
 * @param b_sample_ok Current encoder sample validity.
 */
static void task_motor_update_home(task_motor_context_t *context,
                                   TickType_t now,
                                   bool b_sample_ok)
{
    int32_t current_counts;
    HAL_StatusTypeDef status;

    if (0u == Stepper_IsEmergencyLatched())
    {
        context->b_home_pending = false;
        task_motor_set_priority(false);
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 context->home_command_id);
        return;
    }

    if ((TickType_t)(now - context->home_start_ticks) >=
        pdMS_TO_TICKS(APP_MOTOR_HOME_TIMEOUT_MS))
    {
        context->b_home_pending = false;
        context->b_sample_valid = false;
        context->b_reference_valid = false;
        task_motor_set_priority(false);
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 context->home_command_id);
        return;
    }

    if ((false == b_sample_ok) ||
        (false == context->b_magnet_detected))
    {
        context->b_home_pending = false;
        context->b_sample_valid = false;
        context->b_reference_valid = false;
        task_motor_set_priority(false);
        task_motor_record_report(context,
                                 APP_MOTOR_ERROR,
                                 context->home_command_id);
        return;
    }

    current_counts = HAL_AS5600_GetPosition();

    if (current_counts == context->home_previous_counts)
    {
        context->home_stable_samples++;
    }
    else
    {
        context->home_stable_samples = 1u;
    }

    context->home_previous_counts = current_counts;

    if (context->home_stable_samples >=
        APP_MOTOR_HOME_STABLE_SAMPLES)
    {
        status = HAL_AS5600_SetZero();

        context->b_home_pending = false;
        task_motor_set_priority(false);

        if (HAL_OK == status)
        {
            context->b_sample_valid = true;
            context->b_reference_valid = true;
            context->b_have_sample = true;
            context->last_sample_ticks = now;

            task_motor_record_report(context,
                                     APP_MOTOR_ARRIVED,
                                     context->home_command_id);
        }
        else
        {
            context->b_sample_valid = false;
            task_motor_record_report(context,
                                     APP_MOTOR_ERROR,
                                     context->home_command_id);
        }
    }
}

/*** end of file ***/
