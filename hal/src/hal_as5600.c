/**
 * @file hal_as5600.c
 * @brief HAL driver for AS5600 magnetic rotary encoder.
 */
#include "hal_as5600.h"
#include "bsp_i2c.h"

/* Encoder software state. */
static uint16_t previous_angle = 0U;
static int32_t accumulated_position = 0;
static uint8_t initialized = 0U;

/**
 * @brief Read raw angle from AS5600.
 */
HAL_StatusTypeDef
HAL_AS5600_ReadRawAngle(uint16_t *angle)
{
    uint8_t data[2] = {0U};
    HAL_StatusTypeDef status;

    if (NULL == angle)
    {
        return HAL_ERROR;
    }

    status = BSP_I2C1_ReadRegister(
        HAL_AS5600_I2C_ADDRESS,
        HAL_AS5600_REG_RAW_ANGLE_H,
        data,
        2U);

    if (HAL_OK != status)
    {
        return status;
    }

    /* Combine two bytes and keep only 12 bits. */
    *angle = (uint16_t)((((uint16_t)data[0] << 8U) |
                         (uint16_t)data[1]) & 0x0FFFU);

    return HAL_OK;
}

/**
 * @brief Initialize AS5600 driver.
 */
HAL_StatusTypeDef
HAL_AS5600_Init(void)
{
    uint16_t angle = 0U;
    HAL_StatusTypeDef status;

    status = HAL_AS5600_ReadRawAngle(&angle);

    if (HAL_OK != status)
    {
        initialized = 0U;
        return status;
    }

    previous_angle = angle;
    accumulated_position = 0;
    initialized = 1U;

    return HAL_OK;
}

/**
 * @brief Read angle in degrees.
 */
HAL_StatusTypeDef
HAL_AS5600_ReadAngleDegrees(float *degrees)
{
    uint16_t angle = 0U;
    HAL_StatusTypeDef status;

    if (NULL == degrees)
    {
        return HAL_ERROR;
    }

    status = HAL_AS5600_ReadRawAngle(&angle);

    if (HAL_OK != status)
    {
        return status;
    }

    *degrees = ((float)angle * 360.0f) /
               (float)HAL_AS5600_COUNTS_PER_REV;

    return HAL_OK;
}

/**
 * @brief Update accumulated multi-turn position.
 */
HAL_StatusTypeDef
HAL_AS5600_UpdatePosition(void)
{
    uint16_t current_angle = 0U;
    int32_t delta;
    HAL_StatusTypeDef status;

    if (0U == initialized)
    {
        return HAL_ERROR;
    }

    status = HAL_AS5600_ReadRawAngle(&current_angle);

    if (HAL_OK != status)
    {
        return status;
    }

    delta = (int32_t)current_angle -
            (int32_t)previous_angle;

    /* Detect clockwise wrap-around. */
    if (delta > HAL_AS5600_HALF_REVOLUTION)
    {
        delta -= HAL_AS5600_COUNTS_PER_REV;
    }
    /* Detect counter-clockwise wrap-around. */
    else if (delta < -HAL_AS5600_HALF_REVOLUTION)
    {
        delta += HAL_AS5600_COUNTS_PER_REV;
    }
    else
    {
        /* No wrap-around. */
    }

    accumulated_position += delta;
    previous_angle = current_angle;

    return HAL_OK;
}

/**
 * @brief Get accumulated position in encoder counts.
 */
int32_t
HAL_AS5600_GetPosition(void)
{
    return accumulated_position;
}

/**
 * @brief Get accumulated position in revolutions.
 */
float
HAL_AS5600_GetRevolutions(void)
{
    return (float)accumulated_position /
           (float)HAL_AS5600_COUNTS_PER_REV;
}

/**
 * @brief Set current encoder position as zero reference.
 */
HAL_StatusTypeDef
HAL_AS5600_SetZero(void)
{
    uint16_t angle = 0U;
    HAL_StatusTypeDef status;

    status = HAL_AS5600_ReadRawAngle(&angle);

    if (HAL_OK != status)
    {
        return status;
    }

    previous_angle = angle;
    accumulated_position = 0;
    initialized = 1U;

    return HAL_OK;
}

/**
 * @brief Check whether the magnet is detected.
 */
HAL_StatusTypeDef
HAL_AS5600_IsMagnetDetected(uint8_t *detected)
{
    uint8_t status_register = 0U;
    HAL_StatusTypeDef status;

    if (NULL == detected)
    {
        return HAL_ERROR;
    }

    status = BSP_I2C1_ReadRegister(
        HAL_AS5600_I2C_ADDRESS,
        HAL_AS5600_REG_STATUS,
        &status_register,
        1U);

    if (HAL_OK != status)
    {
        return status;
    }

    /* STATUS.MD is bit 5. */
    *detected = (0U != (status_register & 0x20U)) ? 1U : 0U;

    return HAL_OK;
}

/*** end of file ***/
