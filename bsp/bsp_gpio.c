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

    /* Configure buttons and limit switch: PA0-PA4. */
    pins.Pin = GPIO_PIN_0 |
                          GPIO_PIN_1 |
                          GPIO_PIN_2 |
                          GPIO_PIN_3 |
                          GPIO_PIN_4;

    pins.Mode = GPIO_MODE_INPUT;
    pins.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(GPIOA, &pins);

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
