/** @file main.c
 * @brief Integrated startup; autonomous motion requires measured configuration.
 */
#include "stm32f1xx_hal.h"
#include "bsp_gpio.h"
#include "bsp_i2c.h"
#include "bsp_spi.h"
#include "bsp_tim.h"
#include "app_ipc.h"
#include "hal_stepper.h"
#include "task_motor.h"
#include "task_input.h"
#include "task_display.h"
#include "task_elevator.h"
#include "task_emergency.h"
#include "task.h"

int
main (void)
{
    SystemCoreClockUpdate();
    configASSERT(HAL_OK == HAL_Init());
    BSP_GPIO_Init();
    Stepper_Init();
    configASSERT(HAL_OK == BSP_TIM2_Init());
    configASSERT(HAL_OK == BSP_I2C1_Init());
    configASSERT(HAL_OK == BSP_SPI1_Init());
    configASSERT(pdPASS == app_ipc_init());
    configASSERT(pdPASS == task_emergency_create());
    configASSERT(pdPASS == task_motor_create());
    configASSERT(pdPASS == task_input_create());
    configASSERT(pdPASS == task_display_create());
    configASSERT(pdPASS == task_elevator_create());
    configASSERT(HAL_OK == BSP_TIM2_Start());
    vTaskStartScheduler();
    bsp_fault_stop();
    return 0;
}

/*** end of file ***/
