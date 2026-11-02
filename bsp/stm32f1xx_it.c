#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

_Static_assert(configTICK_RATE_HZ == 1000U, "Shared HAL/RTOS SysTick requires 1 ms ticks");
void xPortSysTickHandler(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
    /* FreeRTOS must also count ticks while its scheduler is suspended. */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}
