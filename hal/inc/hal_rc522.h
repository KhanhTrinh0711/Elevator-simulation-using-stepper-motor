/** @file hal_rc522.h
 * @brief Single-card ISO14443A UID reader over the shared SPI1 BSP.
 */
#ifndef HAL_RC522_H
#define HAL_RC522_H

#include <stdint.h>

#define HAL_RC522_UID_CAPACITY (10u)

/** @brief Distinguish absent cards from bus, protocol and reader failures. */
typedef enum
{
    HAL_RC522_OK = 0,
    HAL_RC522_NO_CARD,
    HAL_RC522_COLLISION,
    HAL_RC522_IO_ERROR,
    HAL_RC522_PROTOCOL_ERROR,
    HAL_RC522_TIMEOUT,
    HAL_RC522_UNSUPPORTED,
    HAL_RC522_NOT_READY,
    HAL_RC522_INVALID_ARGUMENT
} hal_rc522_status_t;

/** @brief UID lengths are 4, 7 or 10; sak is the final select response. */
typedef struct
{
    uint8_t bytes[HAL_RC522_UID_CAPACITY];
    uint8_t length;
    uint8_t sak;
} hal_rc522_uid_t;

/** @brief Reset and initialize a genuine MFRC522 version 0x91 or 0x92.
 * BSP_SPI1_Init must succeed before scheduler start. Call from one owning
 * task with scheduler/tick running and interrupts enabled. No ISR calls.
 * Writes the observed version to p_version even if it is unsupported.
 * Driver uses RTOS delays and bounded polling; module RST must remain high.
 * On failure retry initialization before reading another UID.
 */
hal_rc522_status_t hal_rc522_init (uint8_t * p_version);

/** @brief Wake, select and halt one ISO14443A card, checking BCC and SAK CRC.
 * A held card can be read again on the next poll. No block access, Crypto1,
 * whitelist or elevator authorization is performed. Multiple cards may return
 * COLLISION; collision resolution is deliberately not implemented.
 * Output is cleared on entry and published only after a complete valid read.
 * The task must exclusively own the RC522 across its multi-frame commands;
 * the BSP mutex protects individual SPI frames, not an entire card selection.
 * Timeouts are bounded polling limits, not strict wall-clock deadlines under
 * preemption or bus contention. Reinitialize after IO_ERROR/TIMEOUT.
 */
hal_rc522_status_t hal_rc522_read_uid (hal_rc522_uid_t * p_uid);

#endif /* HAL_RC522_H */

/*** end of file ***/
