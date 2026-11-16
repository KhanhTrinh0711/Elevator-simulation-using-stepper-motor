/** @file hal_oled.h
 * @brief SSD1306 I2C text display and legacy countdown rendering.
 */
#ifndef HAL_OLED_H
#define HAL_OLED_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

#define HAL_OLED_ADDRESS       (0x3Cu)
#define HAL_OLED_WIDTH         (128u)
#define HAL_OLED_HEIGHT        (64u)
#define HAL_OLED_TEXT_COLUMNS  (21u)
#define HAL_OLED_TEXT_ROWS     (8u)
#define HAL_OLED_COUNT_MAX     (10u)

/** @brief Initialize and clear a 128x64 SSD1306 with internal charge pump.
 * BSP_I2C1_Init must succeed before the scheduler starts. Call from the one
 * display task, after module power/reset has stabilized. No reset GPIO is
 * driven: this assumes an I2C module with its own power-on reset circuit.
 * Returns HAL status; on failure retry initialization after a bounded delay.
 */
HAL_StatusTypeDef hal_oled_init (void);

/** @brief Show 0..10 centered, with no leading zero, after successful init.
 * One task must own the OLED. Encoder can share I2C through the BSP mutex.
 * Each packet contains at most 16 data bytes plus its control byte.
 * Returns HAL_ERROR for invalid count, otherwise the first HAL failure.
 * After a bus failure reinitialize before drawing again.
 */
HAL_StatusTypeDef hal_oled_show_count (uint8_t count);

/** @brief Draw one 8-pixel row and erase its unused columns.
 * Accepts a NUL-terminated string of at most 21 characters; row is 0..7.
 * Supports uppercase ASCII, digits, space, colon, dash, dot and question mark.
 * Unsupported characters render as '?'. Returns HAL_ERROR for invalid input
 * without writing. One display owner only; retry init after an I2C failure.
 */
HAL_StatusTypeDef hal_oled_write_line (uint8_t row, const char * p_text);

#endif /* HAL_OLED_H */

/*** end of file ***/
