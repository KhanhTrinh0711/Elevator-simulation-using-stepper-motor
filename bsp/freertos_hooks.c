#include "FreeRTOS.h"
#include "task.h"

void vApplicationMallocFailedHook(void)
{
    configASSERT(0);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    configASSERT(0);
}
