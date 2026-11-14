
/**
 * @file hal_stepper.h
 * @brief Stepper motor driver interface.
 */
#ifndef HAL_STEPPER_H
#define HAL_STEPPER_H

#include "stm32f1xx_hal.h"

typedef enum
{
    STEPPER_DIR_CW = 0,
    STEPPER_DIR_CCW
} Stepper_Direction_t;

/** @brief Initialize the stepper driver. */
void Stepper_Init(void);

/**
 * @brief Start the motor.
 * @param steps Number of phase transitions.
 * @param direction Rotation direction.
 * @return HAL_OK, HAL_BUSY or HAL_ERROR.
 */
HAL_StatusTypeDef Stepper_Start(uint32_t steps,
                                Stepper_Direction_t direction);

/** @brief Stop the motor while retaining the current phase. */
void Stepper_Stop(void);

/** @brief Stop the motor immediately and latch emergency state. */
void Stepper_EmergencyStop(void);

/**
 * @brief Recover from emergency after PA0 is released.
 * @return HAL_OK if recovered, HAL_BUSY otherwise.
 */
HAL_StatusTypeDef Stepper_EmergencyRecover(void);

/** @brief Check whether emergency state is latched. */
uint8_t Stepper_IsEmergencyLatched(void);

/** @brief Check whether the motor is running. */
uint8_t Stepper_IsBusy(void);

/** @brief Check whether a limit-switch event is pending. */
uint8_t Stepper_LimitEventPending(void);

/** @brief Clear the pending limit-switch event. */
void Stepper_ClearLimitEvent(void);

/** @brief Advance the motor state from the TIM2 callback. */
void Stepper_OnTimerElapsed(void);

#endif /* HAL_STEPPER_H */

/*** end of file ***/
