/**
 * @file example_crc.h
 * @brief CRC-16-CCITT helper used only by the examples.
 *
 * The SDLP library does not compute or validate the Frame Error Control Field;
 * error control is the application's responsibility. This helper lets the
 * examples populate and verify the FECF, and is intentionally kept out of the
 * library's public API.
 */

#ifndef EXAMPLE_CRC_H
#define EXAMPLE_CRC_H

#include <stddef.h>
#include <stdint.h>

/** @brief CRC-16-CCITT generator polynomial: x^16 + x^12 + x^5 + 1. */
#define EXAMPLE_CRC16_POLYNOMIAL 0x1021u

/** @brief CRC-16-CCITT shift register preset: all ones. */
#define EXAMPLE_CRC16_INITIAL_VALUE 0xFFFFu

/** @brief Bits shifted through the register for each input octet. */
#define EXAMPLE_CRC16_BITS_PER_OCTET 8

/**
 * @brief Compute a CRC-16-CCITT (::EXAMPLE_CRC16_POLYNOMIAL, preset to
 *        ::EXAMPLE_CRC16_INITIAL_VALUE).
 *
 * @param[in] data   Input octets.
 * @param[in] length Number of octets.
 * @return The 16-bit CRC.
 */
static inline uint16_t example_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = EXAMPLE_CRC16_INITIAL_VALUE;

    for (size_t i = 0; i < length; i++)
    {
        crc = (uint16_t)(crc ^ ((unsigned)data[i] << 8));

        for (int j = 0; j < EXAMPLE_CRC16_BITS_PER_OCTET; j++)
        {
            if (crc & 0x8000u)
            {
                crc = (uint16_t)(((unsigned)crc << 1) ^ EXAMPLE_CRC16_POLYNOMIAL);
            }
            else
            {
                crc = (uint16_t)((unsigned)crc << 1);
            }
        }
    }

    return crc;
}

#endif
