/** @file task_display.h
 * @brief Snapshot-driven OLED and status LED task.
 */
#ifndef TASK_DISPLAY_H
#define TASK_DISPLAY_H

#include "FreeRTOS.h"

#define TASK_DISPLAY_RETRY_MS       (1000u)
#define TASK_DISPLAY_POWER_WAIT_MS  (100u)
#define TASK_DISPLAY_POLL_MS        (50u)
#define TASK_DISPLAY_RED_TOGGLE_MS  (250u)
#define TASK_DISPLAY_STALE_MS       (1000u)

/** @brief Create one DisplayTask before scheduler start.
 * HAL, BSP_I2C1_Init and app_ipc_init must succeed first. Configures only
 * PB14/PB15 as active-high status LEDs, initially off; no motor GPIO changes.
 * Returns pdPASS or pdFAIL for invalid context, missing queue, duplicate or
 * allocation failure (malloc-failed hook may halt). Owns OLED and both LEDs.
 * Consumes APP_QUEUE_DISPLAY; no publisher/invalid/stale data shows NO DATA
 * and turns green off. Last known Emergency keeps red blinking if data stops.
 * Uses APP_DISPLAY_PERIOD_MS refresh, independent 1 s OLED error retry.
 * LED timing is best effort under I2C waits and scheduling, not a hard stop.
 * Never create another OLED/LED writer alongside this task.
 */
BaseType_t task_display_create (void);

#endif /* TASK_DISPLAY_H */

/*** end of file ***/
