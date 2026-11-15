/** @file task_rfid_test.h
 * @brief Standalone RFID hardware test, not the production InputTask.
 */
#ifndef TASK_RFID_TEST_H
#define TASK_RFID_TEST_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "hal_rc522.h"

#define TASK_RFID_POWER_WAIT_MS (50u)
#define TASK_RFID_RETRY_MS      (1000u)

/** @brief Atomic diagnostic copy; UID is valid only when status is OK.
 * successful_reads counts polls, not distinct card presentations.
 * version is the observed VersionReg, including unsupported values.
 */
typedef struct
{
    hal_rc522_status_t status;
    hal_rc522_uid_t    uid;
    TickType_t        timestamp_ticks;
    uint32_t          successful_reads;
    uint8_t           version;
} task_rfid_snapshot_t;

/** @brief Create one standalone test using the InputTask stack/priority budget.
 * Call after HAL_Init and BSP_SPI1_Init, before starting the scheduler.
 * Configures PB14/PB15 as test LEDs; no motor GPIO or EXTI is initialized.
 * Green: UID read successfully. Both off: no card. Red: reader/protocol error.
 * Do not run another RC522 owner or LED-writing test/display task alongside it.
 * Returns pdPASS on creation, pdFAIL for invalid context/duplicate/allocation
 * failure; the existing malloc-failed hook may halt before returning.
 */
BaseType_t task_rfid_test_create (void);

/** @brief Copy the latest result from task context while scheduler is running.
 * Returns pdFAIL for NULL, invalid context or before task creation.
 * Useful for debugger calls or a future UART/OLED consumer; no printf backend
 * is assumed. UID bytes are binary, not a string, and no authorization occurs.
 */
BaseType_t task_rfid_test_snapshot (task_rfid_snapshot_t * p_snapshot);

#endif /* TASK_RFID_TEST_H */

/*** end of file ***/
