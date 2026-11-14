/** @file stm32f1xx_it.c
 * @brief Shared HAL and FreeRTOS system tick.
 */
#include "stm32f1xx_it.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

_Static_assert(1000u == configTICK_RATE_HZ,
               "Shared HAL/RTOS SysTick requires 1 ms ticks");

/* The FreeRTOS port supplies this entry point without a public prototype. */
void xPortSysTickHandler (void);

/** @brief Service both time bases, including a suspended RTOS scheduler. */
void
SysTick_Handler (void)
{
    HAL_IncTick();

    /* FreeRTOS must also count ticks while its scheduler is suspended. */
    if (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState())
    {
        xPortSysTickHandler();
    }
}

/*** end of file ***/
