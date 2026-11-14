/** @file bsp_gpio.c
 * @brief Board GPIO initialization.
 */
#include "bsp_gpio.h"

/** @brief Apply the board GPIO configuration. */
void
BSP_GPIO_Init (void)
{
    GPIO_InitTypeDef pins = {0};

    /* Enable GPIO clocks. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure motor pins: PB10-PB13. */
    pins.Pin = GPIO_PIN_10 |
               GPIO_PIN_11 |
               GPIO_PIN_12 |
               GPIO_PIN_13;

    pins.Mode  = GPIO_MODE_OUTPUT_PP;
    pins.Pull  = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &pins);

    HAL_GPIO_WritePin(GPIOB,
                      GPIO_PIN_10 | GPIO_PIN_11 |
                      GPIO_PIN_12 | GPIO_PIN_13,
                      GPIO_PIN_RESET);

    /* Emergency button: PA0, active low, EXTI falling edge. */
    pins.Pin  = GPIO_PIN_0;
    pins.Mode = GPIO_MODE_IT_FALLING;
    pins.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &pins);

    /* Limit switch: PA1, active low, EXTI falling edge. */
    pins.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOA, &pins);

    /* Floor buttons: PA2-PA4, polling. */
    pins.Pin = GPIO_PIN_2 |
               GPIO_PIN_3 |
               GPIO_PIN_4;
    pins.Mode = GPIO_MODE_INPUT;
    pins.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &pins);

    /* Enable EXTI interrupts for PA0 and PA1. */
    HAL_NVIC_SetPriority(EXTI0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    HAL_NVIC_SetPriority(EXTI1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);

    /* Configure LEDs: PB14-PB15. */
    pins.Pin   = GPIO_PIN_14 | GPIO_PIN_15;
    pins.Mode  = GPIO_MODE_OUTPUT_PP;
    pins.Pull  = GPIO_NOPULL;
    pins.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &pins);

    HAL_GPIO_WritePin(GPIOB,
                      GPIO_PIN_14 | GPIO_PIN_15,
                      GPIO_PIN_RESET);
}

/*** end of file ***/
