/** @file task_display.c
 * @brief Display task creation and countdown lifecycle.
 */
#include "task_display.h"
#include <stdbool.h>
#include "app_cfg.h"
#include "hal_oled.h"
#include "task.h"
#include "stm32f1xx.h"

_Static_assert(APP_DISPLAY_PRIORITY < configMAX_PRIORITIES,
               "Display priority is outside the configured RTOS range");
_Static_assert(TASK_DISPLAY_COUNT_START <= HAL_OLED_COUNT_MAX,
               "Countdown exceeds the supported OLED number range");
_Static_assert(0u < pdMS_TO_TICKS(TASK_DISPLAY_COUNT_PERIOD),
               "Countdown period must be at least one RTOS tick");

static TaskHandle_t gp_display_task = NULL;

static void display_task (void * p_argument);

/** @brief Create the task without starting any peripheral or scheduler. */
BaseType_t
task_display_create (void)
{
    if ((0u != __get_IPSR()) || (0u != __get_PRIMASK()) ||
        (0u != __get_BASEPRI()) || (NULL != gp_display_task) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }

    return xTaskCreate(display_task, "Display", APP_DISPLAY_STACK_WORDS,
                       NULL, APP_DISPLAY_PRIORITY, &gp_display_task);
}

/** @brief Draw one number per second; retry failures without a busy loop. */
static void
display_task (void * p_argument)
{
    /* The required task signature carries no application argument. */
    (void)p_argument;

    uint8_t count   = TASK_DISPLAY_COUNT_START;
    bool    b_ready = false;

    vTaskDelay(pdMS_TO_TICKS(TASK_DISPLAY_POWER_WAIT_MS));

    for (;;)
    {
        HAL_StatusTypeDef status = HAL_OK;

        if (!b_ready)
        {
            status = hal_oled_init();
            b_ready = (HAL_OK == status);
        }

        if (HAL_OK == status)
        {
            status = hal_oled_show_count(count);
        }

        if (HAL_OK != status)
        {
            b_ready = false;
            vTaskDelay(pdMS_TO_TICKS(TASK_DISPLAY_RETRY_MS));
        }
        else
        {
            /* Hold the completed image for a full second, including zero. */
            vTaskDelay(pdMS_TO_TICKS(TASK_DISPLAY_COUNT_PERIOD));

            if (0u == count)
            {
                count = TASK_DISPLAY_COUNT_START;
            }
            else
            {
                count--;
            }
        }
    }
}

/*** end of file ***/
