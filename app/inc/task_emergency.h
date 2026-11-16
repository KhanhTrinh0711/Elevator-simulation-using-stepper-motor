/** @file task_emergency.h
 * @brief Emergency release timing; never owns elevator state or queues.
 */
#ifndef TASK_EMERGENCY_H
#define TASK_EMERGENCY_H

#include "FreeRTOS.h"
#include <stdbool.h>

BaseType_t task_emergency_create (void);
/** @brief Called by EXTI0 after the driver has stopped motor outputs. */
void task_emergency_signal_from_isr (void);
/** @brief Latch a nonrecoverable runtime fault; reset after repairing cause. */
void task_emergency_block_recovery (void);
/** @brief True after a persistent application fault. */
bool task_emergency_is_blocked (void);

#endif /* TASK_EMERGENCY_H */

/*** end of file ***/
