#include "bsp_gpio.h"

void BSP_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure motor pins: PB10-PB13 */
    GPIO_InitStruct.Pin = GPIO_PIN_10 |
                          GPIO_PIN_11 |
                          GPIO_PIN_12 |
                          GPIO_PIN_13;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB,
                      GPIO_PIN_10 | GPIO_PIN_11 |
                      GPIO_PIN_12 | GPIO_PIN_13,
                      GPIO_PIN_RESET);

    /* Configure buttons and limit switch: PA0-PA4 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 |
                          GPIO_PIN_1 |
                          GPIO_PIN_2 |
                          GPIO_PIN_3 |
                          GPIO_PIN_4;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Configure LEDs: PB14-PB15 */
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB,
                      GPIO_PIN_14 | GPIO_PIN_15,
                      GPIO_PIN_RESET);
}
