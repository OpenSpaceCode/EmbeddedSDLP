#include <gtest/gtest.h>

extern "C" {
#include "sdlp_common.h"
}

/* CRC-16-CCITT: known test vector "123456789" -> 0x29B1 */
TEST(CRC16, KnownVector) {
    const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    uint16_t crc = sdlp_crc16(data, sizeof(data));
    EXPECT_EQ(crc, 0x29B1U);
}

TEST(CRC16, EmptyData) {
    /* CRC of zero bytes starting from 0xFFFF, no iterations -> 0xFFFF */
    uint16_t crc = sdlp_crc16(nullptr, 0);
    EXPECT_EQ(crc, 0xFFFFU);
}

TEST(CRC16, SingleByte) {
    /* Verify that different bytes produce different CRC values */
    const uint8_t a[] = {0x00};
    const uint8_t b[] = {0xFF};
    EXPECT_NE(sdlp_crc16(a, 1), sdlp_crc16(b, 1));
}

TEST(CRC16, Deterministic) {
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_EQ(sdlp_crc16(data, sizeof(data)), sdlp_crc16(data, sizeof(data)));
}
