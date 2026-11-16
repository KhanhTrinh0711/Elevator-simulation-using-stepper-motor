
/** @file task_input.h
 * @brief Application input task interface.
 */
#ifndef TASK_INPUT_H
#define TASK_INPUT_H

#include "FreeRTOS.h"

/** @brief Create InputTask before the scheduler starts.
 * Requires initialized application IPC and BSP GPIO/SPI peripherals.
 * Returns pdPASS on success, otherwise pdFAIL.
 */
BaseType_t task_input_create(void);

#endif /* TASK_INPUT_H */

/*** end of file ***/
