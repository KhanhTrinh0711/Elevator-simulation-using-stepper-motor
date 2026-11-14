
/**
 * @file bsp_tim.h
 * @brief Board timer configuration interface.
 */
#ifndef BSP_TIM_H
#define BSP_TIM_H

#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef htim2;

/**
 * @brief Initialize TIM2 to generate a 1 ms interrupt.
 * @return HAL_OK if successful, otherwise HAL_ERROR.
 */
HAL_StatusTypeDef BSP_TIM2_Init(void);

/**
 * @brief Start TIM2 interrupt generation.
 * @return HAL status.
 */
HAL_StatusTypeDef BSP_TIM2_Start(void);

/**
 * @brief Stop TIM2 interrupt generation.
 * @return HAL status.
 */
HAL_StatusTypeDef BSP_TIM2_Stop(void);

#endif /* BSP_TIM_H */

/*** end of file ***/
