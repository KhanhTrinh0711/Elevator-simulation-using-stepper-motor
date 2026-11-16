/** @file task_emergency.c
 * @brief Highest application priority; 10 s hold and stable button release.
 */
#include "task_emergency.h"
#include <stdbool.h>
#include "app_cfg.h"
#include "bsp_gpio.h"
#include "hal_stepper.h"
#include "task.h"

static TaskHandle_t gp_emergency = NULL;
static bool gb_blocked = false;
static void emergency_task (void * p_argument);

BaseType_t
task_emergency_create (void)
{
    if ((NULL != gp_emergency) || (0u != __get_IPSR()) ||
        (0u != __get_PRIMASK()) || (0u != __get_BASEPRI()) ||
        (taskSCHEDULER_NOT_STARTED != xTaskGetSchedulerState()))
    {
        return pdFAIL;
    }
    return xTaskCreate(emergency_task, "Emergency", APP_EMERGENCY_STACK_WORDS,
                       NULL, APP_EMERGENCY_PRIORITY, &gp_emergency);
}

void
task_emergency_signal_from_isr (void)
{
    if ((NULL != gp_emergency) &&
        (taskSCHEDULER_RUNNING == xTaskGetSchedulerState()))
    {
        BaseType_t b_woken = pdFALSE;
        vTaskNotifyGiveFromISR(gp_emergency, &b_woken);
        portYIELD_FROM_ISR(b_woken);
    }
}

void
task_emergency_block_recovery (void)
{
    taskENTER_CRITICAL();
    gb_blocked = true;
    Stepper_EmergencyStop();
    taskEXIT_CRITICAL();
}

bool
task_emergency_is_blocked (void)
{
    taskENTER_CRITICAL();
    const bool b_blocked = gb_blocked;
    taskEXIT_CRITICAL();
    return b_blocked;
}

static void
emergency_task (void * p_argument)
{
    (void)p_argument;
    uint32_t generation = Stepper_EmergencyGeneration();
    TickType_t stopped = xTaskGetTickCount();
    TickType_t released = stopped;
    bool b_released = false;
    for (;;)
    {
        const TickType_t now = xTaskGetTickCount();
        const uint32_t current = Stepper_EmergencyGeneration();
        if (generation != current)
        {
            generation = current;
            stopped = now;
            b_released = false;
        }
        const bool b_high =
            (GPIO_PIN_SET == HAL_GPIO_ReadPin(GPIOA, BUTTON_EMERGENCY_PIN));
        if (!b_high)
        {
            b_released = false;
        }
        else if (!b_released)
        {
            released = now;
            b_released = true;
        }
        else
        {
            /* Keep the original stable-release timestamp. */
        }
        taskENTER_CRITICAL();
        if ((!gb_blocked) && (0u != Stepper_IsEmergencyLatched()) &&
            b_released && ((TickType_t)(now - stopped) >=
                           pdMS_TO_TICKS(APP_EMERGENCY_WAIT_MS)) &&
            ((TickType_t)(now - released) >=
             pdMS_TO_TICKS(APP_EMERGENCY_RELEASE_MS)))
        {
            /* Driver rejects a newer IRQ generation or pending EXTI edge. */
            (void)Stepper_RecoverGeneration(generation);
        }
        taskEXIT_CRITICAL();
        (void)ulTaskNotifyTake(pdTRUE,
                              pdMS_TO_TICKS(APP_CONTROL_PERIOD_MS));
    }
}

/*** end of file ***/
