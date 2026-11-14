
#include "app_test_gpio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx_hal.h"

#define TEST_GPIO_STACK_SIZE    128
#define TEST_GPIO_PRIORITY      2

static void Task_Test_GPIO(void *argument)
{
    (void)argument;

    while (1)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(500));

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void APP_Test_GPIO_Init(void)
{
    xTaskCreate(
        Task_Test_GPIO,
        "TestGPIO",
        TEST_GPIO_STACK_SIZE,
        NULL,
        TEST_GPIO_PRIORITY,
        NULL
    );
}
