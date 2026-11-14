/**
 * @file hal_as5600.h
 * @brief HAL driver for AS5600 magnetic rotary encoder.
 */
#ifndef HAL_AS5600_H
#define HAL_AS5600_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* AS5600 7-bit I2C address. */
#define HAL_AS5600_I2C_ADDRESS          0x36U

/* AS5600 register addresses. */
#define HAL_AS5600_REG_STATUS           0x0BU
#define HAL_AS5600_REG_RAW_ANGLE_H      0x0CU

/* Encoder resolution. */
#define HAL_AS5600_COUNTS_PER_REV       4096
#define HAL_AS5600_HALF_REVOLUTION      2048

/**
 * @brief Initialize AS5600 driver.
 * BSP_I2C1_Init() must be called before this function.
 */
HAL_StatusTypeDef HAL_AS5600_Init(void);

/** @brief Read raw angle, range 0..4095. */
HAL_StatusTypeDef HAL_AS5600_ReadRawAngle(uint16_t *angle);

/** @brief Read angle in degrees, range approximately 0..359.91. */
HAL_StatusTypeDef HAL_AS5600_ReadAngleDegrees(float *degrees);

/**
 * @brief Update accumulated multi-turn position.
 * Call periodically from MotorTask.
 */
HAL_StatusTypeDef HAL_AS5600_UpdatePosition(void);

/** @brief Get accumulated position in encoder counts. */
int32_t HAL_AS5600_GetPosition(void);

/** @brief Get accumulated position in revolutions. */
float HAL_AS5600_GetRevolutions(void);

/** @brief Set current encoder position as zero reference. */
HAL_StatusTypeDef HAL_AS5600_SetZero(void);

/** @brief Check whether the magnet is detected. */
HAL_StatusTypeDef HAL_AS5600_IsMagnetDetected(uint8_t *detected);

#endif /* HAL_AS5600_H */
