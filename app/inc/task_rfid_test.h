/** @file task_rfid_test.h
 * @brief RFID input policy; historical filename retained for compatibility.
 */
#ifndef TASK_RFID_TEST_H
#define TASK_RFID_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "hal_rc522.h"

#define TASK_RFID_POWER_WAIT_MS  (50u)
#define TASK_RFID_RETRY_MS       (1000u)
#define TASK_RFID_RELEASE_POLLS  (3u)

/** @brief Elevator-owned admission state, copied atomically.
 * Disable in INIT/HOMING/EMERGENCY or admission lock. Advance admission_epoch
 * on every lock/recovery transition; authorization_epoch on auth changes.
 */
typedef struct
{
    uint32_t authorization_epoch;
    uint32_t admission_epoch;
    bool     b_enabled;
} task_rfid_context_t;

/** @brief Diagnostic state; a detected card is not an active access grant.
 * uid is valid only when status == HAL_RC522_OK. successful_reads counts
 * polls; presentations counts cards after confirmed removal. sent_events
 * counts queued AUTH events for valid card reads. Queue success does not
 * mean Elevator accepted the event. dropped_events includes locked scans.
 */
typedef struct
{
    hal_rc522_status_t status;
    hal_rc522_uid_t    uid;
    TickType_t        timestamp_ticks;
    uint32_t          successful_reads;
    uint32_t          presentations;
    uint32_t          sent_events;
    uint32_t          dropped_events;
    uint8_t           version;
} task_rfid_snapshot_t;

/** @brief Configure once before the scheduler, after HAL/SPI and IPC init.
 * Any successfully read UID is accepted in this project phase. UID allowlist
 * filtering is deferred. Returns pdFAIL for invalid context, missing input
 * queue or repeated configuration. Admission starts disabled.
 */
BaseType_t task_rfid_configure (void);

/** @brief Publish admission state from ElevatorTask, not from an ISR.
 * Initially disabled. Does not clear card-presence latch or queued events.
 * Elevator must still reject stale epochs/expired timestamps when receiving.
 */
BaseType_t task_rfid_set_context (const task_rfid_context_t * p_context);

/** @brief Poll from InputTask; call at its 10 ms cadence before button work.
 * Internally rate-limited to APP_RFID_PERIOD_MS with bounded driver waits.
 * First caller owns RC522; other tasks are rejected. Do not also create the
 * standalone task. Returns pdPASS for a valid poll (including no work due),
 * pdFAIL for invalid use; device/queue results are in the diagnostic snapshot.
 * One AUTH per presentation. Three consecutive NO_CARD polls rearm; errors,
 * held cards, queue overflow and lock changes never rearm or retry an event.
 */
BaseType_t task_rfid_poll (void);

/** @brief Optional standalone adapter for module verification only.
 * Configure first; create before scheduler. Uses InputTask's resource budget.
 * No GPIO/LED ownership. Production InputTask calls poll instead of creating
 * this adapter, so the final application still has five tasks.
 */
BaseType_t task_rfid_test_create (void);

/** @brief Copy diagnostics from a running task after configuration. */
BaseType_t task_rfid_test_snapshot (task_rfid_snapshot_t * p_snapshot);

#endif /* TASK_RFID_TEST_H */

/*** end of file ***/
