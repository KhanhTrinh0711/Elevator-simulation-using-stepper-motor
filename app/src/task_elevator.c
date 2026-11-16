/** @file task_elevator.c
 * @brief Three-floor FIFO dispatch, authorization and recovery orchestration.
 */
#include "task_elevator.h"
#include <stdbool.h>
#include "app_cfg.h"
#include "app_ipc.h"
#include "hal_stepper.h"
#include "task_emergency.h"
#include "task_motor.h"
#include "task.h"

#define ELEVATOR_SENSOR_WAIT_MS (2000u)

typedef struct
{
    app_state_t state;
    app_error_t error;
    app_direction_t direction;
    app_input_context_t input;
    app_position_t position;
    TickType_t auth_start;
    TickType_t sensor_wait;
    TickType_t display_time;
    uint32_t command_id;
    uint32_t generation;
    uint8_t current_floor;
    uint8_t target;
    bool b_authorized;
    bool b_locked;
    bool b_queued[APP_FLOOR_COUNT];
} elevator_context_t;

static TaskHandle_t gp_elevator = NULL;
static elevator_context_t g_elevator;
static QueueHandle_t gh_input = NULL;
static QueueHandle_t gh_floors = NULL;
static QueueHandle_t gh_commands = NULL;
static QueueHandle_t gh_reports = NULL;
static QueueHandle_t gh_position = NULL;
static QueueHandle_t gh_display = NULL;
static const int32_t g_targets[APP_FLOOR_COUNT] =
    {0, APP_FLOOR_TWO_COUNTS, APP_FLOOR_THREE_COUNTS};

static void elevator_task (void * p_argument);
static void elevator_context (elevator_context_t * p_lift);
static void elevator_cancel (elevator_context_t * p_lift);
static void elevator_fault (elevator_context_t * p_lift, app_error_t error);
static void elevator_command (elevator_context_t * p_lift,
                              app_motor_action_t action, uint8_t floor);
static void elevator_report (elevator_context_t * p_lift);
static void elevator_input (elevator_context_t * p_lift, TickType_t now);
static void elevator_display (elevator_context_t * p_lift, TickType_t now);

BaseType_t
task_elevator_create (void)
{
    if ((NULL != gp_elevator) || (0u != __get_IPSR()) ||
        (0u != __get_PRIMASK()) || (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    gh_input = app_ipc_queue(APP_QUEUE_INPUT);
    gh_floors = app_ipc_queue(APP_QUEUE_FLOOR);
    gh_commands = app_ipc_queue(APP_QUEUE_MOTOR_COMMAND);
    gh_reports = app_ipc_queue(APP_QUEUE_MOTOR_REPORT);
    gh_position = app_ipc_queue(APP_QUEUE_POSITION);
    gh_display = app_ipc_queue(APP_QUEUE_DISPLAY);
    if ((NULL == gh_input) || (NULL == gh_floors) || (NULL == gh_commands) ||
        (NULL == gh_reports) || (NULL == gh_position) || (NULL == gh_display))
    {
        return pdFAIL;
    }
    return xTaskCreate(elevator_task, "Elevator", APP_ELEVATOR_STACK_WORDS,
                       NULL, APP_ELEVATOR_PRIORITY, &gp_elevator);
}

/** @brief Every admission transition starts a new input session. */
static void
elevator_context (elevator_context_t * p_lift)
{
    const bool b_open = ((!p_lift->b_locked) &&
        ((APP_STATE_IDLE == p_lift->state) ||
         (APP_STATE_MOVING == p_lift->state) ||
         (APP_STATE_ARRIVED == p_lift->state)) &&
        (0u == Stepper_IsEmergencyLatched()));
    if (b_open != p_lift->input.b_admission_open)
    {
        p_lift->input.admission_epoch++;
        p_lift->input.b_admission_open = b_open;
    }
    configASSERT(pdPASS == app_ipc_input_context_publish(&p_lift->input));
}

/** @brief Discard old requests; let MotorTask drain its reports. */
static void
elevator_cancel (elevator_context_t * p_lift)
{
    p_lift->state = APP_STATE_EMERGENCY;
    p_lift->direction = APP_DIRECTION_STOPPED;
    p_lift->current_floor = 0u;
    p_lift->target = 0u;
    p_lift->b_locked = true;
    p_lift->b_authorized = false;
    p_lift->input.authorization_epoch++;
    p_lift->input.admission_epoch++;
    p_lift->command_id++;
    p_lift->position.b_valid = false;
    for (uint32_t idx = 0u; idx < APP_FLOOR_COUNT; idx++)
    {
        p_lift->b_queued[idx] = false;
    }
    (void)xQueueReset(gh_floors);
    (void)xQueueReset(gh_commands);
    (void)xQueueReset(gh_input);
    elevator_context(p_lift);
}

static void
elevator_fault (elevator_context_t * p_lift, app_error_t error)
{
    p_lift->error = error;
    task_emergency_block_recovery();
    p_lift->generation = Stepper_EmergencyGeneration();
    elevator_cancel(p_lift);
}

/** @brief HOME finds a reference; MOVE uses measured floor targets. */
static void
elevator_command (elevator_context_t * p_lift,
                   app_motor_action_t action, uint8_t floor)
{
    const app_motor_command_t command = {
        .action = action,
        .target_counts = g_targets[floor - 1u],
        .command_id = ++p_lift->command_id
    };
    p_lift->target = floor;
    p_lift->state = (APP_MOTOR_HOME == action) ? APP_STATE_HOMING :
                                                APP_STATE_MOVING;
    p_lift->direction = (APP_MOTOR_HOME == action) ?
        APP_DIRECTION_DOWN : ((command.target_counts > p_lift->position.counts)
        ? APP_DIRECTION_UP : APP_DIRECTION_DOWN);
    p_lift->current_floor = 0u;
    elevator_context(p_lift);
    if (pdPASS != xQueueSend(gh_commands, &command, 0u))
    {
        elevator_fault(p_lift, APP_ERROR_IPC);
    }
}

/** @brief Reports from canceled command IDs cannot complete a new journey. */
static void
elevator_report (elevator_context_t * p_lift)
{
    app_motor_report_t report = {0};
    for (uint32_t idx = 0u; idx < APP_MOTOR_RPT_QUEUE_LENGTH; idx++)
    {
        if (pdPASS != xQueueReceive(gh_reports, &report, 0u))
        {
            break;
        }
        if ((report.command_id != p_lift->command_id) ||
            (APP_STATE_EMERGENCY == p_lift->state))
        {
            continue;
        }
        if ((APP_MOTOR_ARRIVED != report.result) || (!report.position.b_valid))
        {
            elevator_fault(p_lift, APP_ERROR_MOTOR);
            return;
        }
        const bool b_home = (APP_STATE_HOMING == p_lift->state);
        /* A delayed terminal report must not replace newer telemetry. */
        if ((TickType_t)(report.position.timestamp_ticks -
                        p_lift->position.timestamp_ticks) < (UINT32_MAX / 2u))
        {
            p_lift->position = report.position;
        }
        p_lift->current_floor = p_lift->target;
        p_lift->target = 0u;
        p_lift->direction = APP_DIRECTION_STOPPED;
        p_lift->state = APP_STATE_ARRIVED;
        uint8_t next = 0u;
        if ((!b_home) && (pdPASS == xQueueReceive(gh_floors, &next, 0u)))
        {
            p_lift->b_queued[next - 1u] = false;
            elevator_command(p_lift, APP_MOTOR_MOVE, next);
        }
        else
        {
            p_lift->state = APP_STATE_IDLE;
            p_lift->b_locked = false;
            elevator_context(p_lift);
        }
    }
}

/** @brief Accept FIFO requests only in the current authorization/admission. */
static void
elevator_input (elevator_context_t * p_lift, TickType_t now)
{
    app_input_event_t event = {0};
    for (uint32_t idx = 0u; idx < APP_INPUT_QUEUE_LENGTH; idx++)
    {
        if (pdPASS != xQueueReceive(gh_input, &event, 0u))
        {
            break;
        }
        if ((!p_lift->input.b_admission_open) ||
            (0u != Stepper_IsEmergencyLatched()) ||
            (event.admission_epoch != p_lift->input.admission_epoch) ||
            (event.authorization_epoch != p_lift->input.authorization_epoch))
        {
            continue;
        }
        if (APP_INPUT_AUTH == event.kind)
        {
            if (event.b_authorized &&
                ((4u == event.uid_length) || (7u == event.uid_length) ||
                 (10u == event.uid_length)) &&
                ((TickType_t)(now - event.timestamp_ticks) <
                 pdMS_TO_TICKS(APP_AUTH_WINDOW_MS)))
            {
                p_lift->b_authorized = true;
                p_lift->auth_start = event.timestamp_ticks;
                p_lift->input.authorization_epoch++;
                elevator_context(p_lift);
            }
            continue;
        }
        if ((APP_INPUT_FLOOR != event.kind) || (!p_lift->b_authorized) ||
            (1u > event.floor) || (APP_FLOOR_COUNT < event.floor) ||
            ((TickType_t)(event.timestamp_ticks - p_lift->auth_start) >=
             pdMS_TO_TICKS(APP_AUTH_WINDOW_MS)) ||
            ((TickType_t)(now - p_lift->auth_start) >=
             pdMS_TO_TICKS(APP_AUTH_WINDOW_MS)) ||
            (event.floor == p_lift->target) ||
            p_lift->b_queued[event.floor - 1u])
        {
            continue;
        }
        if (APP_STATE_IDLE == p_lift->state)
        {
            if (event.floor != p_lift->current_floor)
            {
                elevator_command(p_lift, APP_MOTOR_MOVE, event.floor);
            }
        }
        else if (pdPASS == xQueueSend(gh_floors, &event.floor, 0u))
        {
            p_lift->b_queued[event.floor - 1u] = true;
            if (APP_FLOOR_QUEUE_LENGTH == uxQueueMessagesWaiting(gh_floors))
            {
                p_lift->b_locked = true;
                elevator_context(p_lift);
            }
        }
        else
        {
            elevator_fault(p_lift, APP_ERROR_IPC);
            return;
        }
    }
}

static void
elevator_display (elevator_context_t * p_lift, TickType_t now)
{
    const TickType_t age = now - p_lift->auth_start;
    const uint32_t remaining = (p_lift->b_authorized &&
        (age < pdMS_TO_TICKS(APP_AUTH_WINDOW_MS))) ?
        (APP_AUTH_WINDOW_MS - (age * portTICK_PERIOD_MS)) : 0u;
    const app_display_snapshot_t snapshot = {
        .timestamp_ticks = now,
        .state = p_lift->state,
        .error = p_lift->error,
        .direction = p_lift->direction,
        .auth_remaining_ms = remaining,
        .current_floor = p_lift->current_floor,
        /* The floor queue is bounded to two entries. */
        .waiting_count = (uint8_t)uxQueueMessagesWaiting(gh_floors),
        .b_ready = ((APP_STATE_IDLE == p_lift->state) &&
                    (!p_lift->b_locked) && (0u == p_lift->target))
    };
    configASSERT(pdPASS == xQueueOverwrite(gh_display, &snapshot));
    p_lift->display_time = now;
}

static void
elevator_task (void * p_argument)
{
    (void)p_argument;
    elevator_context_t * p_lift = &g_elevator;
    p_lift->sensor_wait = xTaskGetTickCount();
    p_lift->generation = Stepper_EmergencyGeneration();
    if (!task_motor_configured())
    {
        elevator_fault(p_lift, APP_ERROR_CONFIG);
    }
    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        const app_state_t previous = p_lift->state;
        (void)xQueueReceive(gh_position, &p_lift->position, 0u);
        const uint32_t generation = Stepper_EmergencyGeneration();
        if ((generation != p_lift->generation) ||
            ((0u != Stepper_IsEmergencyLatched()) &&
             (APP_STATE_EMERGENCY != p_lift->state)))
        {
            p_lift->generation = generation;
            if ((APP_ERROR_NONE == p_lift->error) &&
                task_emergency_is_blocked())
            {
                p_lift->error = APP_ERROR_MOTOR;
            }
            elevator_cancel(p_lift);
        }
        if ((APP_STATE_EMERGENCY == p_lift->state) &&
            (APP_ERROR_NONE == p_lift->error) &&
            (0u == Stepper_IsEmergencyLatched()))
        {
            p_lift->state = APP_STATE_INIT;
            p_lift->sensor_wait = now;
        }
        now = xTaskGetTickCount();
        if (APP_STATE_INIT == p_lift->state)
        {
            if (p_lift->position.b_sensor_valid &&
                ((TickType_t)(now - p_lift->position.timestamp_ticks) <=
                 pdMS_TO_TICKS(APP_POSITION_MAX_GAP_MS)))
            {
                elevator_command(p_lift, APP_MOTOR_HOME, 1u);
            }
            else if ((TickType_t)(now - p_lift->sensor_wait) >=
                     pdMS_TO_TICKS(ELEVATOR_SENSOR_WAIT_MS))
            {
                elevator_fault(p_lift, APP_ERROR_SENSOR);
            }
        }
        elevator_report(p_lift);
        now = xTaskGetTickCount();
        if ((APP_STATE_IDLE == p_lift->state) &&
            ((!p_lift->position.b_valid) ||
             ((TickType_t)(now - p_lift->position.timestamp_ticks) >
              pdMS_TO_TICKS(APP_POSITION_MAX_GAP_MS))))
        {
            elevator_fault(p_lift, APP_ERROR_SENSOR);
        }
        elevator_input(p_lift, now);
        if ((APP_STATE_HOMING == p_lift->state) &&
            p_lift->position.b_sensor_valid)
        {
            p_lift->direction = p_lift->position.direction;
        }
        if ((previous != p_lift->state) ||
            ((TickType_t)(now - p_lift->display_time) >=
             pdMS_TO_TICKS(APP_DISPLAY_PERIOD_MS)))
        {
            elevator_display(p_lift, now);
        }
        vTaskDelay(pdMS_TO_TICKS(APP_CONTROL_PERIOD_MS));
    }
}

/*** end of file ***/
