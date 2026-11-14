/** @file main.c
 * @brief Standalone HAL LED example before application integration.
 */
#include "main.h"
#include "stm32f1xx_hal.h"

/** @brief Initialize HAL and toggle the board LED indefinitely. */
int
main (void)
{
    /* Reset uses HSI at 8 MHz; no external crystal is required. */
    SystemCoreClockUpdate();
    if (HAL_OK != HAL_Init())
    {
        __disable_irq();
        for (;;)
        {
            /* Stop here because the HAL time base could not be initialized. */
        }
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitTypeDef led = {0};

    led.Pin   = GPIO_PIN_13;
    led.Mode  = GPIO_MODE_OUTPUT_PP;
    led.Pull  = GPIO_NOPULL;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &led);

    for (;;)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(500u);
    }
}

/*** end of file ***/
