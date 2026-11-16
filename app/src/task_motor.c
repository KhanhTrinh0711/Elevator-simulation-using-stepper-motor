/** @file task_motor.c
 * @brief Sole owner of motion, encoder samples and bounded floor-1 homing.
 */
#include "task_motor.h"
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_as5600.h"
#include "hal_stepper.h"
#include "task_emergency.h"
#include "task.h"

#define MOTOR_REPORT_SLOTS (2u)

typedef enum
{
    MOTOR_IDLE = 0,
    MOTOR_MOVE,
    MOTOR_RELEASE,
    MOTOR_RELEASE_SETTLE,
    MOTOR_SEARCH,
    MOTOR_VERIFY
} motor_phase_t;

typedef struct
{
    motor_phase_t phase;
    bool b_sensor;
    bool b_sample;
    bool b_reference;
    bool b_have_sample;
    int32_t position;
    int32_t target;
    int32_t start;
    int32_t progress;
    TickType_t sample_start;
    TickType_t sample_time;
    TickType_t retry;
    TickType_t started;
    TickType_t progress_time;
    TickType_t settled;
    bool b_settling;
    uint32_t command_id;
    uint32_t last_id;
    uint32_t generation;
    app_motor_report_t reports[MOTOR_REPORT_SLOTS];
    uint32_t report_count;
} motor_context_t;

static TaskHandle_t gp_motor = NULL;
static motor_context_t g_motor;
static QueueHandle_t gh_commands = NULL;
static QueueHandle_t gh_reports = NULL;
static QueueHandle_t gh_position = NULL;

static void motor_task (void * p_argument);
static void motor_sample (motor_context_t * p_motor);
static void motor_report (motor_context_t * p_motor,
                          app_motor_result_t result, uint32_t id);
static void motor_finish (motor_context_t * p_motor,
                          app_motor_result_t result);
static bool motor_start (motor_context_t * p_motor, uint32_t steps,
                         bool b_up, bool b_home);
static void motor_command (motor_context_t * p_motor);
static void motor_update (motor_context_t * p_motor);
static void motor_home_update (motor_context_t * p_motor, TickType_t now);
static void motor_fail (motor_context_t * p_motor);

/** @brief A zero unconfigured bound expires immediately if reached. */
static bool
motor_expired (TickType_t now, TickType_t start, uint32_t timeout_ms)
{
    return ((TickType_t)(now - start) >= pdMS_TO_TICKS(timeout_ms));
}

/** @brief Measurements must be complete before any autonomous motion. */
bool
task_motor_configured (void)
{
    return ((1u == APP_MOTOR_MECHANICS_CALIBRATED) &&
        (0u < APP_MOTOR_STEPS_PER_ENCODER_REV) &&
        (1u >= APP_MOTOR_POSITIVE_DIR_CW) &&
        (0u < APP_MOTOR_MOVE_TIMEOUT_MS) &&
        (0u < APP_MOTOR_STEP_INTERVAL_MS) &&
        (UINT16_MAX >= APP_MOTOR_STEP_INTERVAL_MS) &&
        (0u < APP_HOME_STEP_INTERVAL_MS) &&
        (UINT16_MAX >= APP_HOME_STEP_INTERVAL_MS) &&
        (0u < APP_HOME_SEARCH_STEPS) && (0u < APP_HOME_RELEASE_STEPS) &&
        (0u < APP_HOME_SEARCH_TIMEOUT_MS) &&
        (0u < APP_HOME_RELEASE_TIMEOUT_MS) &&
        (0u < APP_MOTOR_STALL_TIMEOUT_MS) &&
        (0 < APP_FLOOR_TWO_COUNTS) &&
        (APP_FLOOR_TWO_COUNTS < APP_FLOOR_THREE_COUNTS));
}

BaseType_t
task_motor_create (void)
{
    if ((NULL != gp_motor) || (0u != __get_IPSR()) ||
        (0u != __get_PRIMASK()) || (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    gh_commands = app_ipc_queue(APP_QUEUE_MOTOR_COMMAND);
    gh_reports = app_ipc_queue(APP_QUEUE_MOTOR_REPORT);
    gh_position = app_ipc_queue(APP_QUEUE_POSITION);
    if ((NULL == gh_commands) || (NULL == gh_reports) ||
        (NULL == gh_position))
    {
        return pdFAIL;
    }
    g_motor.generation = Stepper_EmergencyGeneration();
    return xTaskCreate(motor_task, "Motor", APP_MOTOR_STACK_WORDS, NULL,
                       APP_MOTOR_PRIORITY_IDLE, &gp_motor);
}

/** @brief Keep terminal reports until delivery without overwriting. */
static void
motor_report (motor_context_t * p_motor, app_motor_result_t result,
              uint32_t id)
{
    configASSERT(p_motor->report_count < MOTOR_REPORT_SLOTS);
    app_motor_report_t * p_report = &p_motor->reports[p_motor->report_count];
    p_report->command_id = id;
    p_report->result = result;
    p_report->position.direction = APP_DIRECTION_STOPPED;
    p_report->position.counts = p_motor->position;
    p_report->position.timestamp_ticks = p_motor->sample_time;
    p_report->position.b_sensor_valid = p_motor->b_sample;
    p_report->position.b_valid = p_motor->b_sample && p_motor->b_reference;
    p_motor->report_count++;
}

/** @brief Return to idle only after reserving the terminal result. */
static void
motor_finish (motor_context_t * p_motor, app_motor_result_t result)
{
    Stepper_Stop();
    motor_report(p_motor, result, p_motor->command_id);
    p_motor->phase = MOTOR_IDLE;
    vTaskPrioritySet(NULL, APP_MOTOR_PRIORITY_IDLE);
}

static void
motor_fail (motor_context_t * p_motor)
{
    p_motor->b_reference = false;
    if (0u != Stepper_IsEmergencyLatched())
    {
        motor_finish(p_motor, APP_MOTOR_STOPPED);
    }
    else
    {
        task_emergency_block_recovery();
        motor_finish(p_motor, APP_MOTOR_ERROR);
    }
}

/** @brief Include bus wait in sample validity, not just loop start times. */
static void
motor_sample (motor_context_t * p_motor)
{
    TickType_t start = xTaskGetTickCount();
    p_motor->b_sample = false;
    if (!p_motor->b_sensor)
    {
        if ((TickType_t)(start - p_motor->retry) <
            pdMS_TO_TICKS(APP_SENSOR_RETRY_MS))
        {
            return;
        }
        p_motor->retry = start;
        p_motor->b_reference = false;
        p_motor->b_have_sample = false;
        p_motor->b_sensor = (HAL_OK == HAL_AS5600_Init());
        if (!p_motor->b_sensor)
        {
            return;
        }
    }
    start = xTaskGetTickCount();
    uint8_t magnet = 0u;
    const HAL_StatusTypeDef angle = HAL_AS5600_UpdatePosition();
    const HAL_StatusTypeDef field = HAL_AS5600_IsMagnetDetected(&magnet);
    const TickType_t finish = xTaskGetTickCount();
    /* From the previous read START to this read END is a conservative bound. */
    const TickType_t since = p_motor->b_have_sample ?
                             p_motor->sample_start : start;
    p_motor->b_sample = ((HAL_OK == angle) && (HAL_OK == field) &&
        (0u != magnet) && ((TickType_t)(finish - since) <=
                          pdMS_TO_TICKS(APP_POSITION_MAX_GAP_MS)));
    p_motor->sample_start = start;
    p_motor->sample_time = finish;
    p_motor->b_have_sample = true;
    if (!p_motor->b_sample)
    {
        p_motor->b_reference = false;
        p_motor->b_sensor = false;
    }
    if (HAL_OK == angle)
    {
        p_motor->position = HAL_AS5600_GetPosition();
    }
}

/** @brief Direction uses the measured mapping; downward motion guards PA1. */
static bool
motor_start (motor_context_t * p_motor, uint32_t steps,
              bool b_up, bool b_home)
{
    const bool b_clockwise = (b_up == (1u == APP_MOTOR_POSITIVE_DIR_CW));
    const uint32_t interval = b_home ? APP_HOME_STEP_INTERVAL_MS :
                                      APP_MOTOR_STEP_INTERVAL_MS;
    /* Config validation guarantees interval fits the 16-bit timer count. */
    if (HAL_OK != Stepper_StartMotion(steps,
            b_clockwise ? STEPPER_DIR_CW : STEPPER_DIR_CCW,
            (uint16_t)interval, !b_up))
    {
        return false;
    }
    p_motor->started = xTaskGetTickCount();
    p_motor->start = p_motor->position;
    p_motor->progress = p_motor->position;
    p_motor->progress_time = p_motor->started;
    p_motor->b_settling = false;
    vTaskPrioritySet(NULL, APP_MOTOR_PRIORITY_ACTIVE);
    return true;
}

/** @brief Two slots retain a rejection and the active command result. */
static void
motor_command (motor_context_t * p_motor)
{
    if (0u != p_motor->report_count)
    {
        return;
    }
    app_motor_command_t command = {0};
    if (pdPASS != xQueueReceive(gh_commands, &command, 0u))
    {
        return;
    }
    const uint32_t difference = command.command_id - p_motor->last_id;
    if ((0u == difference) || (difference >= (UINT32_MAX / 2u)))
    {
        motor_report(p_motor, APP_MOTOR_ERROR, command.command_id);
        return;
    }
    p_motor->last_id = command.command_id;
    if (APP_MOTOR_STOP == command.action)
    {
        if (MOTOR_IDLE != p_motor->phase)
        {
            motor_finish(p_motor, APP_MOTOR_STOPPED);
        }
        else
        {
            motor_report(p_motor, APP_MOTOR_STOPPED, command.command_id);
        }
        return;
    }
    if ((MOTOR_IDLE != p_motor->phase) || (!task_motor_configured()) ||
        (!p_motor->b_sample) || (0u != Stepper_IsEmergencyLatched()))
    {
        motor_report(p_motor, APP_MOTOR_ERROR, command.command_id);
        return;
    }
    p_motor->command_id = command.command_id;
    if (APP_MOTOR_HOME == command.action)
    {
        p_motor->b_reference = false;
        const bool b_release = Stepper_IsLimitActive();
        p_motor->phase = b_release ? MOTOR_RELEASE : MOTOR_SEARCH;
        if (!motor_start(p_motor, b_release ? APP_HOME_RELEASE_STEPS :
                          APP_HOME_SEARCH_STEPS, b_release, true))
        {
            motor_fail(p_motor);
        }
        return;
    }
    if ((APP_MOTOR_MOVE != command.action) || (!p_motor->b_reference) ||
        ((0 != command.target_counts) &&
         (APP_FLOOR_TWO_COUNTS != command.target_counts) &&
         (APP_FLOOR_THREE_COUNTS != command.target_counts)))
    {
        motor_report(p_motor, APP_MOTOR_ERROR, command.command_id);
        return;
    }
    const int64_t delta = (int64_t)command.target_counts - p_motor->position;
    if (0 == delta)
    {
        motor_report(p_motor, APP_MOTOR_ARRIVED, command.command_id);
        return;
    }
    /* Absolute delta of two int32_t values always fits uint64_t. */
    const uint64_t distance = (uint64_t)((delta < 0) ? -delta : delta);
    const uint64_t steps = ((distance * APP_MOTOR_STEPS_PER_ENCODER_REV) +
        (HAL_AS5600_COUNTS_PER_REV - 1u)) / HAL_AS5600_COUNTS_PER_REV;
    if ((0u == steps) || (UINT32_MAX < steps))
    {
        motor_report(p_motor, APP_MOTOR_ERROR, command.command_id);
        return;
    }
    p_motor->target = command.target_counts;
    p_motor->phase = MOTOR_MOVE;
    /* Range validated above before narrowing. */
    if (!motor_start(p_motor, (uint32_t)steps, delta > 0, false))
    {
        motor_fail(p_motor);
    }
}

/** @brief Release, approach and verify are independently bounded phases. */
static void
motor_home_update (motor_context_t * p_motor, TickType_t now)
{
    const bool b_limit = Stepper_IsLimitActive();
    if (MOTOR_RELEASE == p_motor->phase)
    {
        if (!b_limit)
        {
            Stepper_Stop();
            p_motor->phase = MOTOR_RELEASE_SETTLE;
            p_motor->settled = now;
        }
        else if ((0u == Stepper_IsBusy()) ||
                 motor_expired(now, p_motor->started,
                               APP_HOME_RELEASE_TIMEOUT_MS))
        {
            motor_fail(p_motor);
        }
    }
    else if (MOTOR_RELEASE_SETTLE == p_motor->phase)
    {
        if (b_limit)
        {
            motor_fail(p_motor);
        }
        else if ((TickType_t)(now - p_motor->settled) >=
                 pdMS_TO_TICKS(APP_HOME_SETTLE_MS))
        {
            p_motor->phase = MOTOR_SEARCH;
            if (!motor_start(p_motor, APP_HOME_SEARCH_STEPS, false, true))
            {
                motor_fail(p_motor);
            }
        }
    }
    else if (MOTOR_SEARCH == p_motor->phase)
    {
        if (b_limit)
        {
            Stepper_Stop();
            p_motor->phase = MOTOR_VERIFY;
            p_motor->started = now;
            p_motor->settled = now;
            p_motor->progress = p_motor->position;
        }
        else if ((0u == Stepper_IsBusy()) ||
                 motor_expired(now, p_motor->started,
                               APP_HOME_SEARCH_TIMEOUT_MS))
        {
            motor_fail(p_motor);
        }
    }
    else if (MOTOR_VERIFY == p_motor->phase)
    {
        if ((!b_limit) || ((TickType_t)(now - p_motor->started) >=
                          pdMS_TO_TICKS(APP_HOME_VERIFY_TIMEOUT_MS)))
        {
            motor_fail(p_motor);
            return;
        }
        if (p_motor->position != p_motor->progress)
        {
            p_motor->progress = p_motor->position;
            p_motor->settled = now;
        }
        if ((TickType_t)(now - p_motor->settled) >=
            pdMS_TO_TICKS(APP_HOME_SETTLE_MS))
        {
            const TickType_t zero_start = xTaskGetTickCount();
            if ((HAL_OK != HAL_AS5600_SetZero()) ||
                ((TickType_t)(xTaskGetTickCount() - zero_start) >
                 pdMS_TO_TICKS(APP_POSITION_MAX_GAP_MS)) ||
                (!Stepper_IsLimitActive()) ||
                (0u != Stepper_IsEmergencyLatched()))
            {
                motor_fail(p_motor);
                return;
            }
            p_motor->position = 0;
            p_motor->sample_time = xTaskGetTickCount();
            p_motor->sample_start = zero_start;
            p_motor->b_reference = true;
            motor_finish(p_motor, APP_MOTOR_ARRIVED);
        }
    }
    else
    {
        motor_fail(p_motor);
    }
}

/** @brief Watch position, sensor health, deadline and progress. */
static void
motor_update (motor_context_t * p_motor)
{
    if (MOTOR_IDLE == p_motor->phase)
    {
        return;
    }
    const TickType_t now = xTaskGetTickCount();
    if (!p_motor->b_sample)
    {
        motor_fail(p_motor);
        return;
    }
    if (p_motor->position != p_motor->progress)
    {
        p_motor->progress_time = now;
        if (MOTOR_VERIFY != p_motor->phase)
        {
            p_motor->progress = p_motor->position;
        }
    }
    if (0u != Stepper_IsBusy())
    {
        const bool b_up = (MOTOR_RELEASE == p_motor->phase) ||
            ((MOTOR_MOVE == p_motor->phase) &&
             (p_motor->target > p_motor->start));
        const int64_t travel = (int64_t)p_motor->position - p_motor->start;
        const int64_t tolerance = APP_MOTOR_ARRIVAL_TOLERANCE_COUNTS;
        if ((b_up && (travel < -tolerance)) ||
            ((!b_up) && (travel > tolerance)))
        {
            motor_fail(p_motor);
            return;
        }
    }
    if ((0u != Stepper_IsBusy()) &&
        motor_expired(now, p_motor->progress_time,
                      APP_MOTOR_STALL_TIMEOUT_MS))
    {
        motor_fail(p_motor);
        return;
    }
    if (MOTOR_MOVE != p_motor->phase)
    {
        motor_home_update(p_motor, now);
        return;
    }
    const int64_t error = (int64_t)p_motor->target - p_motor->position;
    const bool b_arrived = (p_motor->target > p_motor->start) ?
                           (error <= 0) : (error >= 0);
    /* Magnitude of int32_t subtraction fits uint64_t. */
    const uint64_t distance = (uint64_t)((error < 0) ? -error : error);
    if (distance <= APP_MOTOR_ARRIVAL_TOLERANCE_COUNTS)
    {
        motor_finish(p_motor, APP_MOTOR_ARRIVED);
    }
    else if (b_arrived || (0u == Stepper_IsBusy()) ||
             motor_expired(now, p_motor->started, APP_MOTOR_MOVE_TIMEOUT_MS))
    {
        motor_fail(p_motor);
    }
}

static void
motor_task (void * p_argument)
{
    (void)p_argument;
    TickType_t wake = xTaskGetTickCount();
    for (;;)
    {
        const uint32_t generation = Stepper_EmergencyGeneration();
        if ((generation != g_motor.generation) ||
            (0u != Stepper_IsEmergencyLatched()))
        {
            if (generation != g_motor.generation)
            {
                (void)xQueueReset(gh_commands);
            }
            g_motor.generation = generation;
            g_motor.b_reference = false;
            if (MOTOR_IDLE != g_motor.phase)
            {
                motor_finish(&g_motor, APP_MOTOR_STOPPED);
            }
        }
        motor_sample(&g_motor);
        if (0u == Stepper_IsEmergencyLatched())
        {
            motor_update(&g_motor);
        }
        if ((0u != g_motor.report_count) &&
            (pdPASS == xQueueSend(gh_reports, &g_motor.reports[0], 0u)))
        {
            g_motor.report_count--;
            g_motor.reports[0] = g_motor.reports[1];
        }
        if (0u == Stepper_IsEmergencyLatched())
        {
            motor_command(&g_motor);
        }
        const bool b_up = (MOTOR_RELEASE == g_motor.phase) ||
            ((MOTOR_MOVE == g_motor.phase) &&
             (g_motor.target > g_motor.start));
        const app_position_t sample = {
            .direction = (0u == Stepper_IsBusy()) ? APP_DIRECTION_STOPPED :
                (b_up ? APP_DIRECTION_UP : APP_DIRECTION_DOWN),
            .counts = g_motor.position,
            .timestamp_ticks = g_motor.sample_time,
            .b_valid = g_motor.b_sample && g_motor.b_reference,
            .b_sensor_valid = g_motor.b_sample
        };
        configASSERT(pdPASS == xQueueOverwrite(gh_position, &sample));
        /* Slow bus operations must not cause rapid catch-up sampling. */
        wake = xTaskGetTickCount();
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(APP_POSITION_PERIOD_MS));
    }
}

/*** end of file ***/
