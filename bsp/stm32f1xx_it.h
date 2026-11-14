/** @file stm32f1xx_it.h
 * @brief Application-owned system interrupt entry point.
 */
#ifndef STM32F1XX_IT_H
#define STM32F1XX_IT_H

/** @brief Advance the HAL tick and, after startup, the FreeRTOS tick. */
void SysTick_Handler (void);

#endif /* STM32F1XX_IT_H */

/*** end of file ***/
