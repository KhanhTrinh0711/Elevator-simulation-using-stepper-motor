
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_tim.h"
#include "hal_stepper.h"

_Static_assert(configTICK_RATE_HZ == 1000U,
               "Shared HAL/RTOS SysTick requires 1 ms ticks");

void xPortSysTickHandler(void);

/* FreeRTOS SysTick */
void SysTick_Handler(void)
{
    HAL_IncTick();

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}

/* TIM2 interrupt */
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/* Emergency button: PA0 */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/* Limit switch: PA1 */
void EXTI1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

