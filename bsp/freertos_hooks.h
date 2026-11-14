/** @file freertos_hooks.h
 * @brief Required FreeRTOS failure callbacks.
 */
#ifndef FREERTOS_HOOKS_H
#define FREERTOS_HOOKS_H

#include "FreeRTOS.h"
#include "task.h"

/** @brief Halt when the configured heap cannot satisfy an allocation. */
void vApplicationMallocFailedHook (void);

/** @brief Halt when FreeRTOS detects a task stack overflow. */
void vApplicationStackOverflowHook (TaskHandle_t p_task, char * p_name);

#endif /* FREERTOS_HOOKS_H */

/*** end of file ***/
