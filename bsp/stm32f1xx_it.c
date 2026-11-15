/** @file stm32f1xx_it.c
 * @brief Shared HAL and FreeRTOS system tick.
 */
#include "stm32f1xx_it.h"
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_tim.h"

_Static_assert(configTICK_RATE_HZ == 1000U,
               "Shared HAL/RTOS SysTick requires 1 ms ticks");

/* The FreeRTOS port supplies this entry point without a public prototype. */
void xPortSysTickHandler(void);

/* Shared HAL and FreeRTOS SysTick. */
void SysTick_Handler(void)
{
    HAL_IncTick();

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}

/* TIM2 interrupt: stepper motor control. */
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/* Emergency button: PA0. */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/* Limit switch: PA1. */
void EXTI1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

/*** end of file ***/
