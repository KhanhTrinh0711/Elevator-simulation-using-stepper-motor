
#ifndef BSP_TIM_H
#define BSP_TIM_H

#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef htim2;

HAL_StatusTypeDef BSP_TIM2_Init(void);
HAL_StatusTypeDef BSP_TIM2_Start(void);
HAL_StatusTypeDef BSP_TIM2_Stop(void);

#endif

