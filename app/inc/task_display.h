/** @file task_display.h
 * @brief Independent OLED countdown task for hardware bring-up.
 */
#ifndef TASK_DISPLAY_H
#define TASK_DISPLAY_H

#include "FreeRTOS.h"

#define TASK_DISPLAY_COUNT_START    (10u)
#define TASK_DISPLAY_COUNT_PERIOD   (1000u)
#define TASK_DISPLAY_RETRY_MS       (1000u)
#define TASK_DISPLAY_POWER_WAIT_MS  (100u)

/** @brief Create one DisplayTask using the budget in app_cfg.h.
 * Call once after BSP_I2C1_Init succeeds, before starting the scheduler.
 * Returns pdPASS on creation; pdFAIL for invalid context, duplicate creation
 * or allocation failure (the configured malloc-failed hook can halt first).
 * The task owns OLED access; do not create a second OLED test task.
 * Shows 10..0, holds zero for one period, then starts at 10 again. On an I2C
 * error it waits, reinitializes OLED and retries the same count.
 * This demo does not consume APP_QUEUE_DISPLAY or operate the status LEDs.
 */
BaseType_t task_display_create (void);

#endif /* TASK_DISPLAY_H */

/*** end of file ***/
