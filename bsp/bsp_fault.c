/** @file bsp_fault.c
 * @brief Fatal shutdown independent of scheduler, heap and driver state.
 */
#include "bsp_fault.h"
#include "stm32f1xx.h"

void
bsp_fault_stop (void)
{
    __disable_irq();
    if (0u != (RCC->APB1ENR & RCC_APB1ENR_TIM2EN))
    {
        TIM2->DIER = 0u;
        TIM2->CR1 &= ~TIM_CR1_CEN;
    }
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    (void)RCC->APB2ENR;
    GPIOB->BRR = (1u << 10u) | (1u << 11u) | (1u << 12u) | (1u << 13u);
    for (;;)
    {
        /* Outputs are off; wait for a debugger or external reset. */
    }
}

/*** end of file ***/
