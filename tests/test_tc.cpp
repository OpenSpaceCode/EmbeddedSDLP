#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "sdlp_tc.h"
}

/* Helper: build and encode a minimal TC frame. */
static int make_encoded_tc(uint8_t *buf, size_t buf_size, size_t *out_size) {
    static const uint8_t payload[] = {0x01};
    sdlp_tc_frame_t frame;
    int r = sdlp_tc_create_frame(&frame, 0x123, 1, 42, payload, sizeof(payload));
    if (r != SDLP_SUCCESS) return r;
    return sdlp_tc_encode_frame(&frame, buf, buf_size, out_size);
}

/* ---------- sdlp_tc_create_frame ---------- */

TEST(TcCreateFrame, Success) {
    const uint8_t data[] = {0x01, 0x02};
    sdlp_tc_frame_t frame;
    int r = sdlp_tc_create_frame(&frame, 0x123, 1, 42, data, sizeof(data));
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.header.spacecraft_id, 0x123U);
    EXPECT_EQ(frame.header.virtual_channel_id, 1U);
    EXPECT_EQ(frame.header.frame_sequence_number, 42U);
    EXPECT_EQ(frame.data_length, (uint16_t)sizeof(data));
    EXPECT_EQ(memcmp(frame.data, data, sizeof(data)), 0);
}

TEST(TcCreateFrame, SpacecraftIdMasked) {
    const uint8_t data[] = {0x00};
    sdlp_tc_frame_t frame;
    /* 0x7FF has 11 bits; only lower 10 should be stored */
    int r = sdlp_tc_create_frame(&frame, 0x7FF, 0, 0, data, sizeof(data));
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.header.spacecraft_id, 0x3FFU);
}

TEST(TcCreateFrame, VirtualChannelIdMasked) {
    const uint8_t data[] = {0x00};
    sdlp_tc_frame_t frame;
    /* 0xFF has 8 bits; only lower 6 should be stored */
    int r = sdlp_tc_create_frame(&frame, 0x001, 0xFF, 0, data, sizeof(data));
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.header.virtual_channel_id, 0x3FU);
}

TEST(TcCreateFrame, NullFrame) {
    const uint8_t data[] = {0x01};
    int r = sdlp_tc_create_frame(nullptr, 0x123, 0, 0, data, sizeof(data));
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcCreateFrame, NullData) {
    sdlp_tc_frame_t frame;
    int r = sdlp_tc_create_frame(&frame, 0x123, 0, 0, nullptr, 1);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcCreateFrame, DataTooLarge) {
    sdlp_tc_frame_t frame;
    const uint8_t data[1] = {0};
    int r = sdlp_tc_create_frame(&frame, 0x123, 0, 0, data, TC_MAX_DATA_SIZE + 1);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcCreateFrame, MaxDataSize) {
    sdlp_tc_frame_t frame;
    uint8_t data[TC_MAX_DATA_SIZE] = {};
    int r = sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, TC_MAX_DATA_SIZE);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.data_length, (uint16_t)TC_MAX_DATA_SIZE);
}

/* ---------- sdlp_tc_encode_frame ---------- */

TEST(TcEncodeFrame, Success) {
    const uint8_t payload[] = {0xAB};
    sdlp_tc_frame_t frame;
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, payload, sizeof(payload)), SDLP_SUCCESS);

    uint8_t buf[64];
    size_t encoded;
    int r = sdlp_tc_encode_frame(&frame, buf, sizeof(buf), &encoded);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(encoded, (size_t)(TC_PRIMARY_HEADER_SIZE + sizeof(payload) + TC_FRAME_ERROR_CONTROL_SIZE));
}

TEST(TcEncodeFrame, NullFrame) {
    uint8_t buf[64];
    size_t encoded;
    int r = sdlp_tc_encode_frame(nullptr, buf, sizeof(buf), &encoded);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcEncodeFrame, NullBuffer) {
    sdlp_tc_frame_t frame;
    const uint8_t data[] = {0x01};
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, sizeof(data)), SDLP_SUCCESS);
    size_t encoded;
    int r = sdlp_tc_encode_frame(&frame, nullptr, 64, &encoded);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcEncodeFrame, NullEncodedSize) {
    sdlp_tc_frame_t frame;
    const uint8_t data[] = {0x01};
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, sizeof(data)), SDLP_SUCCESS);
    uint8_t buf[64];
    int r = sdlp_tc_encode_frame(&frame, buf, sizeof(buf), nullptr);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcEncodeFrame, BufferTooSmall) {
    sdlp_tc_frame_t frame;
    const uint8_t data[] = {0x01};
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, sizeof(data)), SDLP_SUCCESS);
    uint8_t buf[1];
    size_t encoded;
    int r = sdlp_tc_encode_frame(&frame, buf, sizeof(buf), &encoded);
    EXPECT_EQ(r, SDLP_ERROR_BUFFER_TOO_SMALL);
}

/* ---------- sdlp_tc_decode_frame ---------- */

TEST(TcDecodeFrame, Roundtrip) {
    const uint8_t payload[] = {0xCA, 0xFE};
    sdlp_tc_frame_t frame;
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x123, 1, 7, payload, sizeof(payload)), SDLP_SUCCESS);

    uint8_t buf[64];
    size_t encoded;
    ASSERT_EQ(sdlp_tc_encode_frame(&frame, buf, sizeof(buf), &encoded), SDLP_SUCCESS);

    sdlp_tc_frame_t decoded;
    int r = sdlp_tc_decode_frame(buf, encoded, &decoded);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(decoded.header.spacecraft_id, 0x123U);
    EXPECT_EQ(decoded.header.virtual_channel_id, 1U);
    EXPECT_EQ(decoded.header.frame_sequence_number, 7U);
    EXPECT_EQ(decoded.data_length, (uint16_t)sizeof(payload));
    EXPECT_EQ(memcmp(decoded.data, payload, sizeof(payload)), 0);
}

TEST(TcDecodeFrame, NullBuffer) {
    sdlp_tc_frame_t frame;
    uint8_t buf[32];
    size_t enc;
    ASSERT_EQ(make_encoded_tc(buf, sizeof(buf), &enc), SDLP_SUCCESS);
    int r = sdlp_tc_decode_frame(nullptr, enc, &frame);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcDecodeFrame, NullFrame) {
    uint8_t buf[32];
    size_t enc;
    ASSERT_EQ(make_encoded_tc(buf, sizeof(buf), &enc), SDLP_SUCCESS);
    int r = sdlp_tc_decode_frame(buf, enc, nullptr);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcDecodeFrame, BufferTooSmall) {
    sdlp_tc_frame_t frame;
    uint8_t buf[4] = {0};
    int r = sdlp_tc_decode_frame(buf, sizeof(buf), &frame);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcDecodeFrame, CrcMismatch) {
    uint8_t buf[32];
    size_t enc;
    ASSERT_EQ(make_encoded_tc(buf, sizeof(buf), &enc), SDLP_SUCCESS);

    /* Corrupt a data byte */
    buf[TC_PRIMARY_HEADER_SIZE] ^= 0xFF;

    sdlp_tc_frame_t frame;
    int r = sdlp_tc_decode_frame(buf, enc, &frame);
    EXPECT_EQ(r, SDLP_ERROR_CRC_MISMATCH);
}

TEST(TcDecodeFrame, HeaderFieldsDecoded) {
    const uint8_t payload[] = {0x55};
    sdlp_tc_frame_t frame;
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x2AB, 3, 100, payload, sizeof(payload)), SDLP_SUCCESS);

    uint8_t buf[64];
    size_t encoded;
    ASSERT_EQ(sdlp_tc_encode_frame(&frame, buf, sizeof(buf), &encoded), SDLP_SUCCESS);

    sdlp_tc_frame_t decoded;
    ASSERT_EQ(sdlp_tc_decode_frame(buf, encoded, &decoded), SDLP_SUCCESS);

    EXPECT_EQ(decoded.header.spacecraft_id, 0x2ABU);
    EXPECT_EQ(decoded.header.virtual_channel_id, 3U);
    EXPECT_EQ(decoded.header.frame_sequence_number, 100U);
    EXPECT_EQ(decoded.header.transfer_frame_version, (uint16_t)SDLP_VERSION);
}
