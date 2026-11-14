#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "stm32f1xx_hal.h"

#define BSP_I2C1_TIMEOUT_MS 25U
#define BSP_I2C1_MAX_TRANSFER_SIZE 32U

/* Call once after HAL_Init(), before starting the scheduler. PB6=SCL, PB7=SDA.
 * External pull-ups to 3.3 V are required. Initialization returns HAL_ERROR
 * if the mutex cannot be allocated (the configured malloc hook may halt first).
 */
HAL_StatusTypeDef BSP_I2C1_Init(void);

/* Blocking, serialized transfers; task context or single-threaded startup only.
 * Interrupts and HAL tick must be enabled; scheduler must not be suspended.
 * Address is unshifted 7-bit (0x08..0x77); size is 1..32 bytes.
 * Invalid arguments/context or uninitialized bus: HAL_ERROR.
 * Mutex wait: HAL_TIMEOUT after 25 ms; otherwise return the HAL transfer status.
 * Lock wait and HAL transfer each have a separate 25 ms timeout budget.
 * Do not access I2C1 directly or hold another bus mutex around these calls.
 */
HAL_StatusTypeDef BSP_I2C1_Write(uint8_t address, const uint8_t *data,
                               uint16_t size);
HAL_StatusTypeDef BSP_I2C1_ReadRegister(uint8_t address, uint8_t reg,
                                      uint8_t *data, uint16_t size);

#endif
