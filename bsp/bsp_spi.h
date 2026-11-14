#ifndef BSP_SPI_H
#define BSP_SPI_H

#include "stm32f1xx_hal.h"

#define BSP_SPI1_TIMEOUT_MS 25U
#define BSP_SPI1_MAX_TRANSFER_SIZE 65U

/* Call after HAL_Init()/clock setup, before scheduler. PA5/6/7 = SCK/MISO/MOSI,
 * PB0 = RC522 NSS, PB1 = RC522 reset (released high, no reset pulse).
 * Mode 0, 8-bit MSB first, SCK = PCLK2/16. RC522 requires 3.3 V logic.
 * Check status; the configured malloc-failed hook may halt on mutex allocation.
 */
HAL_StatusTypeDef BSP_SPI1_Init(void);

/* One complete RC522 frame including address and dummy bytes, 1..65 bytes.
 * Non-null TX/RX buffers must each hold size bytes; contents must not overlap.
 * NSS stays low for the whole frame and is released even on transfer failure.
 * Serialized with an internal mutex; HAL polling, no DMA/interrupt transfer.
 * Task or single-threaded startup only, interrupts/tick enabled, scheduler not
 * suspended. Do not access SPI1 or NSS directly from another module.
 * Invalid context/arguments/uninitialized bus: HAL_ERROR. Mutex timeout:
 * HAL_TIMEOUT. Otherwise return HAL status; RX is valid only on HAL_OK.
 * Mutex wait and HAL transaction have separate 25 ms timeout budgets.
 */
HAL_StatusTypeDef BSP_SPI1_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t size);

#endif
