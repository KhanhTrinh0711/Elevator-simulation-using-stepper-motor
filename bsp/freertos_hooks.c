/** @file freertos_hooks.c
 * @brief Fail-stop handling for FreeRTOS allocation and stack failures.
 */
#include "freertos_hooks.h"

/** @brief Leave allocation failures visible to the debugger. */
void
vApplicationMallocFailedHook (void)
{
    configASSERT(0);
}

/** @brief Leave stack failures visible to the debugger. */
void
vApplicationStackOverflowHook (TaskHandle_t p_task, char * p_name)
{
    /* Discard callback parameters; casting to void performs no conversion. */
    (void)p_task;
    (void)p_name;
    configASSERT(0);
}

/*** end of file ***/
