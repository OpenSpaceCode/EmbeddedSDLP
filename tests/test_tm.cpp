#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "sdlp_tm.h"
}

/* Helper: create a valid TM frame and encode it into a buffer. */
static int make_encoded_tm(uint8_t *buf, size_t buf_size, size_t *out_size) {
    static const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    sdlp_tm_frame_t frame;
    int r = sdlp_tm_create_frame(&frame, 0x123, 2, payload, sizeof(payload));
    if (r != SDLP_SUCCESS) return r;
    return sdlp_tm_encode_frame(&frame, buf, buf_size, out_size);
}

/* ---------- sdlp_tm_create_frame ---------- */

TEST(TmCreateFrame, Success) {
    const uint8_t data[] = {0x01, 0x02, 0x03};
    sdlp_tm_frame_t frame;
    int r = sdlp_tm_create_frame(&frame, 0x123, 2, data, sizeof(data));
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.header.spacecraft_id, 0x123U);
    EXPECT_EQ(frame.header.virtual_channel_id, 2U);
    EXPECT_EQ(frame.data_length, (uint16_t)sizeof(data));
    EXPECT_EQ(memcmp(frame.data, data, sizeof(data)), 0);
}

TEST(TmCreateFrame, SpacecraftIdMasked) {
    const uint8_t data[] = {0x00};
    sdlp_tm_frame_t frame;
    /* 0x7FF has 11 bits; only lower 10 should be stored */
    int r = sdlp_tm_create_frame(&frame, 0x7FF, 0, data, sizeof(data));
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.header.spacecraft_id, 0x3FFU);
}

TEST(TmCreateFrame, VirtualChannelIdMasked) {
    const uint8_t data[] = {0x00};
    sdlp_tm_frame_t frame;
    /* 0xFF has 8 bits; only lower 3 should be stored */
    int r = sdlp_tm_create_frame(&frame, 0x001, 0xFF, data, sizeof(data));
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.header.virtual_channel_id, 0x07U);
}

TEST(TmCreateFrame, NullFrame) {
    const uint8_t data[] = {0x01};
    int r = sdlp_tm_create_frame(nullptr, 0x123, 0, data, sizeof(data));
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmCreateFrame, NullData) {
    sdlp_tm_frame_t frame;
    int r = sdlp_tm_create_frame(&frame, 0x123, 0, nullptr, 1);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmCreateFrame, DataTooLarge) {
    sdlp_tm_frame_t frame;
    const uint8_t data[1] = {0};
    int r = sdlp_tm_create_frame(&frame, 0x123, 0, data, TM_MAX_DATA_SIZE + 1);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmCreateFrame, MaxDataSize) {
    sdlp_tm_frame_t frame;
    uint8_t data[TM_MAX_DATA_SIZE] = {};
    int r = sdlp_tm_create_frame(&frame, 0x001, 0, data, TM_MAX_DATA_SIZE);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.data_length, (uint16_t)TM_MAX_DATA_SIZE);
}

/* ---------- sdlp_tm_encode_frame ---------- */

TEST(TmEncodeFrame, Success) {
    const uint8_t payload[] = {0x11, 0x22};
    sdlp_tm_frame_t frame;
    ASSERT_EQ(sdlp_tm_create_frame(&frame, 0x001, 0, payload, sizeof(payload)), SDLP_SUCCESS);

    uint8_t buf[64];
    size_t encoded;
    int r = sdlp_tm_encode_frame(&frame, buf, sizeof(buf), &encoded);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(encoded, (size_t)(TM_PRIMARY_HEADER_SIZE + sizeof(payload) + TM_FRAME_ERROR_CONTROL_SIZE));
}

TEST(TmEncodeFrame, NullFrame) {
    uint8_t buf[64];
    size_t encoded;
    int r = sdlp_tm_encode_frame(nullptr, buf, sizeof(buf), &encoded);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmEncodeFrame, NullBuffer) {
    sdlp_tm_frame_t frame;
    const uint8_t data[] = {0x01};
    ASSERT_EQ(sdlp_tm_create_frame(&frame, 0x001, 0, data, sizeof(data)), SDLP_SUCCESS);
    size_t encoded;
    int r = sdlp_tm_encode_frame(&frame, nullptr, 64, &encoded);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmEncodeFrame, NullEncodedSize) {
    sdlp_tm_frame_t frame;
    const uint8_t data[] = {0x01};
    ASSERT_EQ(sdlp_tm_create_frame(&frame, 0x001, 0, data, sizeof(data)), SDLP_SUCCESS);
    uint8_t buf[64];
    int r = sdlp_tm_encode_frame(&frame, buf, sizeof(buf), nullptr);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmEncodeFrame, BufferTooSmall) {
    const uint8_t payload[] = {0x11, 0x22};
    sdlp_tm_frame_t frame;
    ASSERT_EQ(sdlp_tm_create_frame(&frame, 0x001, 0, payload, sizeof(payload)), SDLP_SUCCESS);

    uint8_t buf[1];
    size_t encoded;
    int r = sdlp_tm_encode_frame(&frame, buf, sizeof(buf), &encoded);
    EXPECT_EQ(r, SDLP_ERROR_BUFFER_TOO_SMALL);
}

/* ---------- sdlp_tm_decode_frame ---------- */

TEST(TmDecodeFrame, Roundtrip) {
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    sdlp_tm_frame_t frame;
    ASSERT_EQ(sdlp_tm_create_frame(&frame, 0x123, 3, payload, sizeof(payload)), SDLP_SUCCESS);

    uint8_t buf[64];
    size_t encoded;
    ASSERT_EQ(sdlp_tm_encode_frame(&frame, buf, sizeof(buf), &encoded), SDLP_SUCCESS);

    sdlp_tm_frame_t decoded;
    int r = sdlp_tm_decode_frame(buf, encoded, &decoded);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(decoded.header.spacecraft_id, 0x123U);
    EXPECT_EQ(decoded.header.virtual_channel_id, 3U);
    EXPECT_EQ(decoded.data_length, (uint16_t)sizeof(payload));
    EXPECT_EQ(memcmp(decoded.data, payload, sizeof(payload)), 0);
}

TEST(TmDecodeFrame, NullBuffer) {
    sdlp_tm_frame_t frame;
    uint8_t buf[16];
    size_t enc;
    ASSERT_EQ(make_encoded_tm(buf, sizeof(buf), &enc), SDLP_SUCCESS);
    int r = sdlp_tm_decode_frame(nullptr, enc, &frame);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmDecodeFrame, NullFrame) {
    uint8_t buf[64];
    size_t enc;
    ASSERT_EQ(make_encoded_tm(buf, sizeof(buf), &enc), SDLP_SUCCESS);
    int r = sdlp_tm_decode_frame(buf, enc, nullptr);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmDecodeFrame, BufferTooSmall) {
    sdlp_tm_frame_t frame;
    /* Buffer smaller than header + FECF */
    uint8_t buf[4] = {0};
    int r = sdlp_tm_decode_frame(buf, sizeof(buf), &frame);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TmDecodeFrame, CrcMismatch) {
    uint8_t buf[64];
    size_t enc;
    ASSERT_EQ(make_encoded_tm(buf, sizeof(buf), &enc), SDLP_SUCCESS);

    /* Corrupt a data byte */
    buf[TM_PRIMARY_HEADER_SIZE] ^= 0xFF;

    sdlp_tm_frame_t frame;
    int r = sdlp_tm_decode_frame(buf, enc, &frame);
    EXPECT_EQ(r, SDLP_ERROR_CRC_MISMATCH);
}

TEST(TmDecodeFrame, HeaderFieldsDecoded) {
    const uint8_t payload[] = {0x01};
    sdlp_tm_frame_t frame;
    ASSERT_EQ(sdlp_tm_create_frame(&frame, 0x2AB, 5, payload, sizeof(payload)), SDLP_SUCCESS);

    uint8_t buf[64];
    size_t encoded;
    ASSERT_EQ(sdlp_tm_encode_frame(&frame, buf, sizeof(buf), &encoded), SDLP_SUCCESS);

    sdlp_tm_frame_t decoded;
    ASSERT_EQ(sdlp_tm_decode_frame(buf, encoded, &decoded), SDLP_SUCCESS);

    EXPECT_EQ(decoded.header.spacecraft_id, 0x2ABU);
    EXPECT_EQ(decoded.header.virtual_channel_id, 5U);
    EXPECT_EQ(decoded.header.transfer_frame_version, (uint16_t)SDLP_VERSION);
}
