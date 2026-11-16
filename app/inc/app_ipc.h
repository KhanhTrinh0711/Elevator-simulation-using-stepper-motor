
/** @file app_ipc.h
 * @brief Message contracts and queues shared by application tasks.
 */
#ifndef APP_IPC_H
#define APP_IPC_H

#include <stdbool.h>
#include <stdint.h>

#include "app_cfg.h"
#include "FreeRTOS.h"
#include "queue.h"

/** @brief Queue identifiers; use app_ipc_queue after successful init. */
typedef enum
{
    APP_QUEUE_INPUT = 0,
    APP_QUEUE_FLOOR,
    APP_QUEUE_MOTOR_COMMAND,
    APP_QUEUE_MOTOR_REPORT,
    APP_QUEUE_POSITION,
    APP_QUEUE_DISPLAY,
    APP_QUEUE_COUNT
} app_queue_id_t;

/** @brief Input event types sent to ElevatorTask. */
typedef enum
{
    APP_INPUT_AUTH = 0,
    APP_INPUT_FLOOR,
    APP_INPUT_CALIBRATE
} app_input_kind_t;

/** @brief Input events sent by InputTask and interpreted by ElevatorTask.
 *
 * AUTH carries a successfully read UID and b_authorized=true.
 * This flag indicates successful UID reading, not that ElevatorTask
 * has granted access. RFID currently accepts any successfully read UID;
 * whitelist policy is owned by ElevatorTask when configured by the leader.
 *
 * timestamp_ticks uses the RTOS tick clock, not HAL milliseconds.
 * ElevatorTask grants APP_AUTH_WINDOW_MS from the AUTH event timestamp,
 * never from queue receive time. Expired AUTH events must be rejected.
 *
 * ElevatorTask must reject AUTH/FLOOR while hardware emergency is latched
 * or admission is closed. ElevatorTask owns authorization_epoch and
 * admission_epoch. InputTask copies these epochs from the latest published
 * context. Stale events must be rejected after session or admission changes.
 *
 * FLOOR carries floor (1..3). Unused fields must be zero-initialized.
 */
typedef struct
{
    app_input_kind_t kind;
    TickType_t       timestamp_ticks;
    uint32_t         authorization_epoch;
    uint32_t         admission_epoch;
    uint8_t          floor;
    bool             b_authorized;
    uint8_t          uid[APP_INPUT_UID_CAPACITY];
    uint8_t          uid_length;
} app_input_event_t;

/** @brief Input admission/session snapshot owned by ElevatorTask.
 *
 * InputTask reads this snapshot to tag events with current epochs.
 * Admission is closed by default until ElevatorTask publishes a new context.
 */
typedef struct
{
    uint32_t authorization_epoch;
    uint32_t admission_epoch;
    bool     b_admission_open;
} app_input_context_t;

/** @brief Display direction; no raw motor phase state is exposed. */
typedef enum
{
    APP_DIRECTION_STOPPED = 0,
    APP_DIRECTION_UP,
    APP_DIRECTION_DOWN
} app_direction_t;

/** @brief Counts relative to the calibrated floor-1 reference.
 *
 * Floor targets come from measured APP_FLOOR_*_COUNTS. A sample without
 * a valid calibrated reference must set b_valid to false, including
 * after lost turns.
 */
typedef struct
{
    int32_t    counts;
    app_direction_t direction;
    TickType_t timestamp_ticks;
    bool       b_valid;
    /* Sensor health without an absolute reference, for bounded HOMING. */
    bool       b_sensor_valid;
} app_position_t;

/** @brief Ordinary motor commands; emergency uses a separate hard-stop path. */
typedef enum
{
    APP_MOTOR_MOVE = 0,
    APP_MOTOR_STOP,
    APP_MOTOR_HOME
} app_motor_action_t;

/** @brief ElevatorTask sends commands; MotorTask rejects obsolete IDs. */
typedef struct
{
    app_motor_action_t action;
    int32_t            target_counts;
    uint32_t           command_id;
} app_motor_command_t;

/** @brief Terminal reports only; periodic position uses its own mailbox. */
typedef enum
{
    APP_MOTOR_ARRIVED = 0,
    APP_MOTOR_STOPPED,
    APP_MOTOR_ERROR
} app_motor_result_t;

/** @brief A failed report send must be handled, never silently discarded. */
typedef struct
{
    app_motor_result_t result;
    uint32_t           command_id;
    app_position_t     position;
} app_motor_report_t;

/** @brief State is owned exclusively by ElevatorTask. */
typedef enum
{
    APP_STATE_INIT = 0,
    APP_STATE_HOMING,
    APP_STATE_IDLE,
    APP_STATE_MOVING,
    APP_STATE_ARRIVED,
    APP_STATE_EMERGENCY
} app_state_t;

/** @brief Persistent runtime errors require repair and reset. */
typedef enum
{
    APP_ERROR_NONE = 0,
    APP_ERROR_CONFIG,
    APP_ERROR_SENSOR,
    APP_ERROR_MOTOR,
    APP_ERROR_IPC
} app_error_t;

/** @brief Display snapshot published by ElevatorTask.
 *
 * DisplayTask owns OLED and LEDs. current_floor is 0 when unknown or
 * between floors, otherwise 1..3. b_ready includes IDLE, empty waiting
 * queue, no target, no lock and no emergency.
 * timestamp_ticks is sampled when published. Send at least every
 * APP_DISPLAY_PERIOD_MS, including unchanged states. auth_remaining_ms is
 * relative to that timestamp, clamped to APP_AUTH_WINDOW_MS by the producer.
 */
typedef struct
{
    TickType_t      timestamp_ticks;
    app_state_t     state;
    app_error_t     error;
    app_direction_t direction;
    uint32_t        auth_remaining_ms;
    uint8_t         current_floor;
    uint8_t         waiting_count;
    bool            b_ready;
} app_display_snapshot_t;

/** @brief Create all queues once before scheduler start, outside an ISR.
 *
 * Returns pdPASS on success, including repeated startup calls, and pdFAIL
 * on invalid context or allocation failure. No task, mutex or notification
 * is created here. Queue handles are published only after all allocations
 * pass. The current malloc-failed hook may halt before allocation failure
 * returns.
 */
BaseType_t app_ipc_init(void);

/** @brief Return a queue handle, or NULL before init / for an invalid ID.
 *
 * INPUT: app_input_event_t, InputTask -> ElevatorTask.
 * FLOOR: uint8_t (1..3), owned only by ElevatorTask.
 * MOTOR_COMMAND: app_motor_command_t, ElevatorTask -> MotorTask.
 * MOTOR_REPORT: app_motor_report_t, MotorTask -> ElevatorTask.
 * POSITION: app_position_t, MotorTask -> ElevatorTask, overwrite mailbox.
 * DISPLAY: app_display_snapshot_t, ElevatorTask -> DisplayTask, mailbox.
 *
 * Only POSITION and DISPLAY use xQueueOverwrite. Consumers copy messages
 * with receive operations. Do not delete these queues.
 */
QueueHandle_t app_ipc_queue(app_queue_id_t queue_id);

/** @brief Publish the latest input context from ElevatorTask.
 *
 * The context is copied atomically. Call from task context only.
 */
BaseType_t app_ipc_input_context_publish(
    const app_input_context_t *p_context);

/** @brief Read a coherent copy of the latest input context.
 *
 * Returns pdFAIL if IPC is not initialized, arguments are invalid,
 * or the scheduler is not running.
 */
BaseType_t app_ipc_input_context_read(
    app_input_context_t *p_context);

#endif /* APP_IPC_H */

/*** end of file ***/

