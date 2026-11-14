/** @file bsp_i2c.h
 * @brief Board peripheral API.
 */
#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "stm32f1xx_hal.h"

#define BSP_I2C1_TIMEOUT_MS 25u
#define BSP_I2C1_MAX_TRANSFER_SIZE 32u

/** @brief Initialize I2C1.
 * Call once after HAL_Init(), before starting the scheduler. PB6=SCL, PB7=SDA.
 * External pull-ups to 3.3 V are required. Initialization returns HAL_ERROR
 * if the mutex cannot be allocated (the configured malloc hook may halt first).
 */
HAL_StatusTypeDef BSP_I2C1_Init (void);

/** @brief Write one I2C transaction.
 * Blocking, serialized transfers; task context or single-threaded startup only.
 * Interrupts and HAL tick must be enabled; scheduler must not be suspended.
 * Address is unshifted 7-bit (0x08..0x77); size is 1..32 bytes.
 * Invalid arguments/context or uninitialized bus: HAL_ERROR.
 * Mutex wait: HAL_TIMEOUT after 25 ms; otherwise return HAL status.
 * Lock wait and HAL transfer each have a separate 25 ms timeout budget.
 * Do not access I2C1 directly or hold another bus mutex around these calls.
 */
HAL_StatusTypeDef BSP_I2C1_Write (uint8_t address, const uint8_t * p_data,
                               uint16_t size);
/** @brief Read 8-bit registers; same context and timeout contract as Write. */
HAL_StatusTypeDef BSP_I2C1_ReadRegister (uint8_t address, uint8_t reg,
                                      uint8_t * p_data, uint16_t size);

#endif /* BSP_I2C_H */

/*** end of file ***/
