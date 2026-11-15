
#include "app_test_button.h"

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx_hal.h"

#define BUTTON_TASK_STACK_SIZE    128
#define BUTTON_TASK_PRIORITY      2

#define BUTTON_SAMPLE_TIME_MS     20
#define BUTTON_DEBOUNCE_COUNT     3

static void Task_Test_Button(void *argument)
{
    (void)argument;

    uint8_t count = 0;
    uint8_t led_state = 0;
    uint8_t pressed = 0;

    while (1)
    {
        /* Kiem tra ba nut nhan */
        if ((HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET) ||
            (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) ||
            (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET))
        {
            if (count < BUTTON_DEBOUNCE_COUNT)
            {
                count++;
            }
        }
        else
        {
            count = 0;
        }

        /* Loc doi nut nhan */
        pressed = (count >= BUTTON_DEBOUNCE_COUNT);

        if (pressed)
        {
            led_state = !led_state;

            if (led_state)
            {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
            }
            else
            {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
            }

            vTaskDelay(pdMS_TO_TICKS(300));
        }
        else
        {
            led_state = 0;

            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);

            vTaskDelay(pdMS_TO_TICKS(BUTTON_SAMPLE_TIME_MS));
        }
    }
}

void APP_Test_Button_Init(void)
{
    BaseType_t result;

    result = xTaskCreate(
        Task_Test_Button,
        "TestButton",
        BUTTON_TASK_STACK_SIZE,
        NULL,
        BUTTON_TASK_PRIORITY,
        NULL
    );

    configASSERT(result == pdPASS);
}
