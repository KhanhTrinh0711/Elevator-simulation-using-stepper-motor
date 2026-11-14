
#include "app_test_stepper.h"

#include "FreeRTOS.h"
#include "task.h"

#include "hal_stepper.h"

#define STEPPER_TASK_STACK_SIZE    128
#define STEPPER_TASK_PRIORITY      2

#define STEPPER_TEST_STEPS         1024
#define STEPPER_PAUSE_MS           1000

static void Task_Test_Stepper(void *argument)
{
    (void)argument;

    while (1)
    {
        /* Quay thuan */
        if (Stepper_Start(STEPPER_TEST_STEPS,
                          STEPPER_DIR_CW) == HAL_OK)
        {
            while (Stepper_IsBusy())
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            vTaskDelay(pdMS_TO_TICKS(STEPPER_PAUSE_MS));
        }

        /* Quay nguoc */
        if (Stepper_Start(STEPPER_TEST_STEPS,
                          STEPPER_DIR_CCW) == HAL_OK)
        {
            while (Stepper_IsBusy())
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            vTaskDelay(pdMS_TO_TICKS(STEPPER_PAUSE_MS));
        }
    }
}

void APP_Test_Stepper_Init(void)
{
    BaseType_t result;

    result = xTaskCreate(
        Task_Test_Stepper,
        "TestStepper",
        STEPPER_TASK_STACK_SIZE,
        NULL,
        STEPPER_TASK_PRIORITY,
        NULL
    );

    configASSERT(result == pdPASS);
}
