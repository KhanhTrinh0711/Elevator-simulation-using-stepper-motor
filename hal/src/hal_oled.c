/** @file hal_oled.c
 * @brief SSD1306 page writes and small digit rendering without a framebuffer.
 * Commands follow the Solomon Systech SSD1306 datasheet, sections 8 and 10.
 * See docs/DISPLAY_DEMO.md for the source and board assumptions.
 */
#include "hal_oled.h"
#include <stddef.h>
#include "bsp_i2c.h"

#define OLED_PAGE_HEIGHT       (8u)
#define OLED_PAGE_COUNT        (HAL_OLED_HEIGHT / OLED_PAGE_HEIGHT)
#define OLED_CHUNK_SIZE        (16u)
#define OLED_DATA_CONTROL      (0x40u)
#define OLED_PAGE_COMMAND      (0xB0u)
#define OLED_COLUMN_HIGH       (0x10u)
#define OLED_NIBBLE_MASK       (0x0Fu)
#define OLED_NIBBLE_BITS       (4u)
#define OLED_GLYPH_WIDTH       (5u)
#define OLED_GLYPH_HEIGHT      (7u)
#define OLED_DIGIT_COUNT       (10u)
#define OLED_SCALE             (4u)
#define OLED_CELL_WIDTH        (24u)
#define OLED_DRAW_WIDTH        (2u * OLED_CELL_WIDTH)
#define OLED_DRAW_PAGES        (4u)
#define OLED_START_COLUMN      ((HAL_OLED_WIDTH - OLED_DRAW_WIDTH) / 2u)
#define OLED_START_PAGE        (2u)

_Static_assert((OLED_CHUNK_SIZE + 1u) <= BSP_I2C1_MAX_TRANSFER_SIZE,
               "OLED packet exceeds the BSP transfer limit");

/* Five columns per glyph, with the top pixel in bit zero. */
static const uint8_t g_digits[OLED_DIGIT_COUNT][OLED_GLYPH_WIDTH] =
{
    {0x3Eu, 0x51u, 0x49u, 0x45u, 0x3Eu},
    {0x00u, 0x42u, 0x7Fu, 0x40u, 0x00u},
    {0x42u, 0x61u, 0x51u, 0x49u, 0x46u},
    {0x21u, 0x41u, 0x45u, 0x4Bu, 0x31u},
    {0x18u, 0x14u, 0x12u, 0x7Fu, 0x10u},
    {0x27u, 0x45u, 0x45u, 0x45u, 0x39u},
    {0x3Cu, 0x4Au, 0x49u, 0x49u, 0x30u},
    {0x01u, 0x71u, 0x09u, 0x05u, 0x03u},
    {0x36u, 0x49u, 0x49u, 0x49u, 0x36u},
    {0x06u, 0x49u, 0x49u, 0x29u, 0x1Eu}
};

/* Original 5x7 uppercase glyphs, columns with bit zero at the top. */
static const uint8_t g_letters[26u][OLED_GLYPH_WIDTH] =
{
    {0x7Eu, 0x09u, 0x09u, 0x09u, 0x7Eu},
    {0x7Fu, 0x49u, 0x49u, 0x49u, 0x36u},
    {0x3Eu, 0x41u, 0x41u, 0x41u, 0x22u},
    {0x7Fu, 0x41u, 0x41u, 0x22u, 0x1Cu},
    {0x7Fu, 0x49u, 0x49u, 0x49u, 0x41u},
    {0x7Fu, 0x09u, 0x09u, 0x09u, 0x01u},
    {0x3Eu, 0x41u, 0x49u, 0x49u, 0x3Au},
    {0x7Fu, 0x08u, 0x08u, 0x08u, 0x7Fu},
    {0x41u, 0x41u, 0x7Fu, 0x41u, 0x41u},
    {0x20u, 0x40u, 0x41u, 0x3Fu, 0x01u},
    {0x7Fu, 0x08u, 0x14u, 0x22u, 0x41u},
    {0x7Fu, 0x40u, 0x40u, 0x40u, 0x40u},
    {0x7Fu, 0x02u, 0x0Cu, 0x02u, 0x7Fu},
    {0x7Fu, 0x04u, 0x08u, 0x10u, 0x7Fu},
    {0x3Eu, 0x41u, 0x41u, 0x41u, 0x3Eu},
    {0x7Fu, 0x09u, 0x09u, 0x09u, 0x06u},
    {0x3Eu, 0x41u, 0x51u, 0x21u, 0x5Eu},
    {0x7Fu, 0x09u, 0x19u, 0x29u, 0x46u},
    {0x26u, 0x49u, 0x49u, 0x49u, 0x32u},
    {0x01u, 0x01u, 0x7Fu, 0x01u, 0x01u},
    {0x3Fu, 0x40u, 0x40u, 0x40u, 0x3Fu},
    {0x1Fu, 0x20u, 0x40u, 0x20u, 0x1Fu},
    {0x3Fu, 0x40u, 0x38u, 0x40u, 0x3Fu},
    {0x63u, 0x14u, 0x08u, 0x14u, 0x63u},
    {0x03u, 0x04u, 0x78u, 0x04u, 0x03u},
    {0x61u, 0x51u, 0x49u, 0x45u, 0x43u}
};

static uint8_t oled_text_column (char character, uint32_t column);
static HAL_StatusTypeDef oled_set_cursor (uint8_t page, uint8_t column);
static HAL_StatusTypeDef oled_clear (void);
static uint8_t oled_render_byte (uint8_t count, uint32_t column,
                                uint32_t page);

/** @brief Configure the module, clear RAM while off, then enable display. */
HAL_StatusTypeDef
hal_oled_init (void)
{
    /* Control byte, display off, clock, multiplex, offset, start line,
     * charge pump, page mode, mapping, COM layout, contrast, precharge,
     * VCOMH, RAM display, normal pixels, scroll off.
     */
    const uint8_t commands[] =
    {
        0x00u, 0xAEu, 0xD5u, 0x80u, 0xA8u, 0x3Fu, 0xD3u, 0x00u,
        0x40u, 0x8Du, 0x14u, 0x20u, 0x02u, 0xA1u, 0xC8u, 0xDAu,
        0x12u, 0x81u, 0x7Fu, 0xD9u, 0xF1u, 0xDBu, 0x40u, 0xA4u,
        0xA6u, 0x2Eu
    };
    const uint8_t display_on[] = {0x00u, 0xAFu};
    HAL_StatusTypeDef status = BSP_I2C1_Write(
        HAL_OLED_ADDRESS, commands, sizeof(commands));

    if (HAL_OK != status)
    {
        return status;
    }

    status = oled_clear();

    if (HAL_OK != status)
    {
        return status;
    }

    return BSP_I2C1_Write(HAL_OLED_ADDRESS, display_on, sizeof(display_on));
}

/** @brief Update the number area, including spaces from the old number. */
HAL_StatusTypeDef
hal_oled_show_count (uint8_t count)
{
    if (count > HAL_OLED_COUNT_MAX)
    {
        return HAL_ERROR;
    }

    for (uint32_t page = 0u; page < OLED_DRAW_PAGES; page++)
    {
        /* The page index is in 2..5, so it fits the controller's byte field. */
        HAL_StatusTypeDef status = oled_set_cursor(
            (uint8_t)(OLED_START_PAGE + page), OLED_START_COLUMN);

        if (HAL_OK != status)
        {
            return status;
        }

        for (uint32_t column = 0u; column < OLED_DRAW_WIDTH;
             column += OLED_CHUNK_SIZE)
        {
            uint8_t packet[OLED_CHUNK_SIZE + 1u] = {OLED_DATA_CONTROL};

            for (uint32_t idx = 0u; idx < OLED_CHUNK_SIZE; idx++)
            {
                packet[idx + 1u] = oled_render_byte(count, column + idx, page);
            }

            status = BSP_I2C1_Write(HAL_OLED_ADDRESS, packet, sizeof(packet));

            if (HAL_OK != status)
            {
                return status;
            }
        }
    }

    return HAL_OK;
}

/** @brief Stream a complete row in 16-byte chunks without a framebuffer. */
HAL_StatusTypeDef
hal_oled_write_line (uint8_t row, const char * p_text)
{
    if ((HAL_OLED_TEXT_ROWS <= row) || (NULL == p_text))
    {
        return HAL_ERROR;
    }
    uint32_t length = 0u;
    while ((HAL_OLED_TEXT_COLUMNS >= length) && ('\0' != p_text[length]))
    {
        length++;
    }
    if (HAL_OLED_TEXT_COLUMNS < length)
    {
        return HAL_ERROR;
    }
    HAL_StatusTypeDef status = oled_set_cursor(row, 0u);
    if (HAL_OK != status)
    {
        return status;
    }
    for (uint32_t offset = 0u; offset < HAL_OLED_WIDTH;
         offset += OLED_CHUNK_SIZE)
    {
        uint8_t packet[OLED_CHUNK_SIZE + 1u] = {OLED_DATA_CONTROL};
        for (uint32_t idx = 0u; idx < OLED_CHUNK_SIZE; idx++)
        {
            const uint32_t column = offset + idx;
            const uint32_t letter = column / (OLED_GLYPH_WIDTH + 1u);
            if (letter < length)
            {
                packet[idx + 1u] = oled_text_column(p_text[letter],
                    column % (OLED_GLYPH_WIDTH + 1u));
            }
        }
        status = BSP_I2C1_Write(HAL_OLED_ADDRESS, packet, sizeof(packet));
        if (HAL_OK != status)
        {
            return status;
        }
    }
    return HAL_OK;
}

/** @brief Return a column or a blank separator, with '?' as fallback. */
static uint8_t
oled_text_column (char character, uint32_t column)
{
    static const uint8_t question[OLED_GLYPH_WIDTH] =
        {0x02u, 0x01u, 0x51u, 0x09u, 0x06u};
    if ((OLED_GLYPH_WIDTH <= column) || (' ' == character))
    {
        return 0u;
    }
    if ((character >= '0') && (character <= '9'))
    {
        return g_digits[character - '0'][column];
    }
    if ((character >= 'A') && (character <= 'Z'))
    {
        return g_letters[character - 'A'][column];
    }
    switch (character)
    {
        case ':':
            return (2u == column) ? 0x24u : 0u;
        case '-':
            return 0x08u;
        case '.':
            return (2u == column) ? 0x40u : 0u;
        default:
            return question[column];
    }
}

/** @brief Set a page-mode cursor using bounded internal coordinates. */
static HAL_StatusTypeDef
oled_set_cursor (uint8_t page, uint8_t column)
{
    /* All fields are at most 0xB7; the narrowing casts preserve every bit. */
    const uint8_t commands[] =
    {
        0x00u,
        (uint8_t)(OLED_PAGE_COMMAND | (uint32_t)page),
        (uint8_t)((uint32_t)column & OLED_NIBBLE_MASK),
        (uint8_t)(OLED_COLUMN_HIGH | ((uint32_t)column >> OLED_NIBBLE_BITS))
    };

    return BSP_I2C1_Write(HAL_OLED_ADDRESS, commands, sizeof(commands));
}

/** @brief Clear all eight pages in small transactions to release the bus. */
static HAL_StatusTypeDef
oled_clear (void)
{
    const uint8_t blank[OLED_CHUNK_SIZE + 1u] = {OLED_DATA_CONTROL};

    for (uint32_t page = 0u; page < OLED_PAGE_COUNT; page++)
    {
        /* There are eight pages, so conversion to uint8_t is lossless. */
        HAL_StatusTypeDef status = oled_set_cursor((uint8_t)page, 0u);

        if (HAL_OK != status)
        {
            return status;
        }

        for (uint32_t column = 0u; column < HAL_OLED_WIDTH;
             column += OLED_CHUNK_SIZE)
        {
            status = BSP_I2C1_Write(HAL_OLED_ADDRESS, blank, sizeof(blank));

            if (HAL_OK != status)
            {
                return status;
            }
        }
    }

    return HAL_OK;
}

/** @brief Expand a glyph column into one byte of the number area. */
static uint8_t
oled_render_byte (uint8_t count, uint32_t column, uint32_t page)
{
    const uint32_t offset = (count < OLED_DIGIT_COUNT) ?
                            (OLED_CELL_WIDTH / 2u) : 0u;
    uint32_t digit_column = 0u;
    uint32_t digit        = count;
    uint32_t pixels       = 0u;

    if (column < offset)
    {
        return 0u;
    }

    column -= offset;
    digit_column = (column % OLED_CELL_WIDTH) / OLED_SCALE;

    if ((digit_column >= OLED_GLYPH_WIDTH) ||
        ((count < OLED_DIGIT_COUNT) && (column >= OLED_CELL_WIDTH)))
    {
        return 0u;
    }

    if (count >= OLED_DIGIT_COUNT)
    {
        digit = (column < OLED_CELL_WIDTH) ?
                 (count / OLED_DIGIT_COUNT) : (count % OLED_DIGIT_COUNT);
    }

    for (uint32_t bit = 0u; bit < OLED_PAGE_HEIGHT; bit++)
    {
        const uint32_t row = ((page * OLED_PAGE_HEIGHT) + bit) / OLED_SCALE;

        if ((row < OLED_GLYPH_HEIGHT) &&
            (0u != ((uint32_t)g_digits[digit][digit_column] & (1u << row))))
        {
            pixels |= (1u << bit);
        }
    }

    /* Only bits 0..7 are set; narrowing to the display byte is lossless. */
    return (uint8_t)pixels;
}

/*** end of file ***/
