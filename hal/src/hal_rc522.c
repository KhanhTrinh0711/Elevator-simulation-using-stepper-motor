/** @file hal_rc522.c
 * @brief MFRC522 initialization and ISO14443A single-card selection.
 * Register definitions: NXP MFRC522 datasheet, sections 8, 9 and 10.
 * Cascade/CRC rules: NXP AN10834; references are in docs/RFID_TEST.md.
 */
#include "hal_rc522.h"
#include <stdbool.h>
#include <stddef.h>
#include "bsp_spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"

#define RC_COMMAND       (0x01u)
#define RC_IRQ           (0x04u)
#define RC_ERROR         (0x06u)
#define RC_FIFO_DATA     (0x09u)
#define RC_FIFO_LEVEL    (0x0Au)
#define RC_CONTROL       (0x0Cu)
#define RC_FRAMING       (0x0Du)
#define RC_VERSION       (0x37u)
#define RC_IDLE          (0x00u)
#define RC_TRANSCEIVE    (0x0Cu)
#define RC_RESET         (0x0Fu)
#define RC_WAIT_MS       (40u)
#define RC_POLL_MS       (1u)
#define RC_FIFO_CAPACITY (64u)
#define RC_CASCADE_TAG   (0x88u)
#define RC_CASCADE_BIT   (0x04u)
#define RC_LEVEL_COUNT   (3u)
#define RC_BLOCK_SIZE    (5u)
#define RC_SELECT_SIZE   (9u)
#define RC_SAK_SIZE      (3u)
#define RC_CRC_BYTES     (2u)
#define RC_BYTE_BITS     (8u)

_Static_assert(0u < pdMS_TO_TICKS(RC_POLL_MS), "RFID needs a 1 ms RTOS tick");

static bool gb_ready = false;

static bool rc_context_valid (void);
static bool rc_write (uint8_t reg, uint8_t value);
static bool rc_read (uint8_t reg, uint8_t * p_value);
static void rc_crc (const uint8_t * p_data, uint32_t size, uint8_t * p_crc);
static hal_rc522_status_t rc_wait_reply (void);
static hal_rc522_status_t rc_exchange (const uint8_t * p_tx, uint8_t tx_size,
    uint8_t tx_bits, uint8_t * p_rx, uint8_t rx_size);
static hal_rc522_status_t rc_select_level (uint8_t level,
    hal_rc522_uid_t * p_uid, bool * p_more);

/** @brief Perform a bounded software reset and enable the 106 kbit/s field. */
hal_rc522_status_t
hal_rc522_init (uint8_t * p_version)
{
    /* Mode: CRC_A preset; RF: 106 kbit/s, 100% ASK, antenna on.
     * Timer: 13.56 MHz / (2*169+1), reload 1000 => approximately 25 ms.
     */
    const uint8_t settings[][2] =
    {
        {0x11u, 0x3Du}, {0x12u, 0x00u}, {0x13u, 0x00u},
        {0x15u, 0x40u}, {0x2Au, 0x80u}, {0x2Bu, 0xA9u},
        {0x2Cu, 0x03u}, {0x2Du, 0xE8u}, {0x14u, 0x83u}
    };
    uint8_t command = 0u;

    if (NULL == p_version)
    {
        return HAL_RC522_INVALID_ARGUMENT;
    }

    *p_version = 0u;

    if (!rc_context_valid())
    {
        return HAL_RC522_NOT_READY;
    }

    gb_ready = false;

    if (!rc_write(RC_COMMAND, RC_RESET))
    {
        return HAL_RC522_IO_ERROR;
    }

    const TickType_t start = xTaskGetTickCount();

    do
    {
        vTaskDelay(pdMS_TO_TICKS(RC_POLL_MS));

        if (!rc_read(RC_COMMAND, &command))
        {
            return HAL_RC522_IO_ERROR;
        }

        if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(RC_WAIT_MS))
        {
            return HAL_RC522_TIMEOUT;
        }
    } while (0u != ((uint32_t)command & 0x10u));

    if (!rc_read(RC_VERSION, p_version))
    {
        return HAL_RC522_IO_ERROR;
    }

    if ((0x91u != *p_version) && (0x92u != *p_version))
    {
        return HAL_RC522_UNSUPPORTED;
    }

    for (uint32_t idx = 0u; idx < (sizeof(settings) / sizeof(settings[0]));
         idx++)
    {
        if (!rc_write(settings[idx][0], settings[idx][1]))
        {
            return HAL_RC522_IO_ERROR;
        }
    }

    /* Allow the newly enabled RF field to settle before polling a card. */
    vTaskDelay(pdMS_TO_TICKS(5u));
    gb_ready = true;

    return HAL_RC522_OK;
}

/** @brief Select all UID cascade levels and return the card to HALT state. */
hal_rc522_status_t
hal_rc522_read_uid (hal_rc522_uid_t * p_uid)
{
    const uint8_t wake = 0x52u;
    uint8_t atqa[2] = {0u};
    hal_rc522_uid_t uid = {0};
    bool b_more = true;

    if (NULL == p_uid)
    {
        return HAL_RC522_INVALID_ARGUMENT;
    }

    *p_uid = uid;

    if ((!rc_context_valid()) || (!gb_ready))
    {
        return HAL_RC522_NOT_READY;
    }

    hal_rc522_status_t status = rc_exchange(&wake, 1u, 7u, atqa, sizeof(atqa));

    if (HAL_RC522_OK != status)
    {
        return status;
    }

    for (uint32_t level = 0u; (level < RC_LEVEL_COUNT) && b_more; level++)
    {
        /* The bounded cascade level is 0..2, so this conversion is lossless. */
        status = rc_select_level((uint8_t)level, &uid, &b_more);

        if (HAL_RC522_OK != status)
        {
            return status;
        }
    }

    if (b_more)
    {
        return HAL_RC522_PROTOCOL_ERROR;
    }

    uint8_t halt[4] = {0x50u, 0x00u, 0x00u, 0x00u};
    rc_crc(halt, sizeof(halt) - RC_CRC_BYTES, &halt[2]);
    status = rc_exchange(halt, sizeof(halt), 0u, NULL, 0u);

    /* A halted card must remain silent; TimerIRq is the expected result. */
    if (HAL_RC522_NO_CARD != status)
    {
        return (HAL_RC522_OK == status) ? HAL_RC522_PROTOCOL_ERROR : status;
    }

    *p_uid = uid;

    return HAL_RC522_OK;
}

/** @brief Keep all polling and multi-frame state inside the owning task. */
static bool
rc_context_valid (void)
{
    return ((0u == __get_IPSR()) && (0u == __get_PRIMASK()) &&
            (0u == __get_BASEPRI()) &&
            (taskSCHEDULER_RUNNING == xTaskGetSchedulerState()));
}

/** @brief Encode a six-bit register address into one complete SPI frame. */
static bool
rc_write (uint8_t reg, uint8_t value)
{
    /* Internal register constants are <=0x3F; shifted addresses fit a byte. */
    const uint8_t tx[2] = {(uint8_t)((uint32_t)reg << 1u), value};
    uint8_t rx[2] = {0u};

    return (HAL_OK == BSP_SPI1_Transfer(tx, rx, sizeof(tx)));
}

/** @brief Read one register with the read bit and a dummy byte. */
static bool
rc_read (uint8_t reg, uint8_t * p_value)
{
    /* Six address bits plus the read flag fit exactly within eight bits. */
    const uint8_t tx[2] = {(uint8_t)(((uint32_t)reg << 1u) | 0x80u), 0u};
    uint8_t rx[2] = {0u};

    if (HAL_OK != BSP_SPI1_Transfer(tx, rx, sizeof(tx)))
    {
        return false;
    }

    *p_value = rx[1];

    return true;
}

/** @brief Compute ISO14443A CRC: preset 0x6363, reflected polynomial 0x8408. */
static void
rc_crc (const uint8_t * p_data, uint32_t size, uint8_t * p_crc)
{
    uint32_t crc = 0x6363u;

    for (uint32_t idx = 0u; idx < size; idx++)
    {
        crc ^= p_data[idx];

        for (uint32_t bit = 0u; bit < RC_BYTE_BITS; bit++)
        {
            crc = (0u != (crc & 1u)) ? ((crc >> 1u) ^ 0x8408u) : (crc >> 1u);
        }
    }

    /* CRC remains 16-bit; masking explicitly selects its two output bytes. */
    p_crc[0] = (uint8_t)(crc & 0xFFu);
    p_crc[1] = (uint8_t)((crc >> RC_BYTE_BITS) & 0xFFu);
}

/** @brief Yield between register polls; never wait indefinitely for RF data. */
static hal_rc522_status_t
rc_wait_reply (void)
{
    const TickType_t start = xTaskGetTickCount();

    for (;;)
    {
        uint8_t irq = 0u;
        uint8_t error = 0u;

        if ((!rc_read(RC_IRQ, &irq)) || (!rc_read(RC_ERROR, &error)))
        {
            return HAL_RC522_IO_ERROR;
        }

        if (0u != ((uint32_t)error & 0x08u))
        {
            return HAL_RC522_COLLISION;
        }

        /* Overflow, temperature, write, CRC, parity and protocol errors. */
        if (0u != ((uint32_t)error & 0xD7u))
        {
            return HAL_RC522_PROTOCOL_ERROR;
        }

        if (0u != ((uint32_t)irq & 0x20u))
        {
            return HAL_RC522_OK;
        }

        if (0u != ((uint32_t)irq & 0x01u))
        {
            return HAL_RC522_NO_CARD;
        }

        if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(RC_WAIT_MS))
        {
            return HAL_RC522_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(RC_POLL_MS));
    }
}

/** @brief Exchange one frame and require the exact expected byte count. */
static hal_rc522_status_t
rc_exchange (const uint8_t * p_tx, uint8_t tx_size, uint8_t tx_bits,
             uint8_t * p_rx, uint8_t rx_size)
{
    if ((!rc_write(RC_COMMAND, RC_IDLE)) || (!rc_write(RC_IRQ, 0x7Fu)) ||
        (!rc_write(RC_FIFO_LEVEL, 0x80u)) || (!rc_write(RC_FRAMING, tx_bits)))
    {
        return HAL_RC522_IO_ERROR;
    }

    for (uint32_t idx = 0u; idx < tx_size; idx++)
    {
        if (!rc_write(RC_FIFO_DATA, p_tx[idx]))
        {
            return HAL_RC522_IO_ERROR;
        }
    }

    /* tx_bits is 0 or 7; adding StartSend cannot exceed one byte. */
    const uint8_t framing = (uint8_t)((uint32_t)tx_bits | 0x80u);

    if ((!rc_write(RC_COMMAND, RC_TRANSCEIVE)) ||
        (!rc_write(RC_FRAMING, framing)))
    {
        return HAL_RC522_IO_ERROR;
    }

    hal_rc522_status_t status = rc_wait_reply();

    if ((!rc_write(RC_FRAMING, 0u)) || (!rc_write(RC_COMMAND, RC_IDLE)))
    {
        return HAL_RC522_IO_ERROR;
    }

    if (HAL_RC522_OK != status)
    {
        return status;
    }

    uint8_t length = 0u;
    uint8_t control = 0u;

    if ((!rc_read(RC_FIFO_LEVEL, &length)) || (!rc_read(RC_CONTROL, &control)))
    {
        return HAL_RC522_IO_ERROR;
    }

    if ((length > RC_FIFO_CAPACITY) || (length != rx_size) ||
        (0u != ((uint32_t)control & 0x07u)))
    {
        return HAL_RC522_PROTOCOL_ERROR;
    }

    for (uint32_t idx = 0u; idx < rx_size; idx++)
    {
        if (!rc_read(RC_FIFO_DATA, &p_rx[idx]))
        {
            return HAL_RC522_IO_ERROR;
        }
    }

    return HAL_RC522_OK;
}

/** @brief Check one anticollision block and its SELECT acknowledgement. */
static hal_rc522_status_t
rc_select_level (uint8_t level, hal_rc522_uid_t * p_uid, bool * p_more)
{
    const uint8_t selectors[RC_LEVEL_COUNT] = {0x93u, 0x95u, 0x97u};
    uint8_t select[RC_SELECT_SIZE] = {0u};
    uint8_t block[RC_BLOCK_SIZE] = {0u};
    uint8_t sak[RC_SAK_SIZE] = {0u};
    uint8_t crc[RC_CRC_BYTES] = {0u};
    select[0] = selectors[level];
    select[1] = 0x20u;
    hal_rc522_status_t status = rc_exchange(select, 2u, 0u,
                                           block, sizeof(block));

    if (HAL_RC522_OK != status)
    {
        return status;
    }

    /* Widen bytes before XOR to avoid signed integer promotion. */
    const uint32_t bcc = ((uint32_t)block[0] ^ (uint32_t)block[1] ^
                          (uint32_t)block[2] ^ (uint32_t)block[3]);

    if (block[4] != bcc)
    {
        return HAL_RC522_PROTOCOL_ERROR;
    }

    select[1] = 0x70u;

    for (uint32_t idx = 0u; idx < RC_BLOCK_SIZE; idx++)
    {
        select[idx + 2u] = block[idx];
    }

    rc_crc(select, RC_SELECT_SIZE - RC_CRC_BYTES, &select[7]);
    status = rc_exchange(select, sizeof(select), 0u, sak, sizeof(sak));

    if (HAL_RC522_OK != status)
    {
        return status;
    }

    rc_crc(sak, 1u, crc);
    /* CL3 always carries four UID bytes; its first byte may equal 0x88. */
    const bool b_tag = ((level < (RC_LEVEL_COUNT - 1u)) &&
                        (RC_CASCADE_TAG == block[0]));
    *p_more = (0u != ((uint32_t)sak[0] & RC_CASCADE_BIT));

    if ((sak[1] != crc[0]) || (sak[2] != crc[1]) || (b_tag != *p_more) ||
        ((*p_more) && (level == (RC_LEVEL_COUNT - 1u))))
    {
        return HAL_RC522_PROTOCOL_ERROR;
    }

    const uint32_t first = b_tag ? 1u : 0u;

    for (uint32_t idx = first; idx < (RC_BLOCK_SIZE - 1u); idx++)
    {
        p_uid->bytes[p_uid->length] = block[idx];
        p_uid->length++;
    }

    p_uid->sak = sak[0];

    return HAL_RC522_OK;
}

/*** end of file ***/
