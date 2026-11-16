
/**
 * @file hal_stepper.h
 * @brief Stepper motor driver interface.
 */
#ifndef HAL_STEPPER_H
#define HAL_STEPPER_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

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

/** @brief Start bounded motion; timer interval is 1..65535 ms.
 * stop_at_limit stops on the active-low floor-1 switch before any next phase.
 * Only upward homing release/motion may deliberately ignore that switch.
 */
HAL_StatusTypeDef Stepper_StartMotion(uint32_t steps,
    Stepper_Direction_t direction, uint16_t interval_ms, bool stop_at_limit);
/** @brief Emergency generation changes on each new stop request. */
uint32_t Stepper_EmergencyGeneration(void);
/** @brief Recover only the observed generation, with released PA0. */
HAL_StatusTypeDef Stepper_RecoverGeneration(uint32_t generation);
/** @brief Read the actual switch level, including at startup. */
bool Stepper_IsLimitActive(void);

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
