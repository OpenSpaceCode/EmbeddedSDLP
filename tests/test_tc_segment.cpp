#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "sdlp_tc.h"
}

/* ---------- sdlp_tc_set_segment_header ---------- */

TEST(TcSegmentHeader, SetSuccess) {
    const uint8_t data[] = {0x01};
    sdlp_tc_frame_t frame;
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, sizeof(data)), SDLP_SUCCESS);

    int r = sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_NO_SEG, 15);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.segment_header.sequence_flags, (uint8_t)TC_SEQ_FLAG_NO_SEG);
    EXPECT_EQ(frame.segment_header.map_id, 15U);
}

TEST(TcSegmentHeader, NullFrame) {
    int r = sdlp_tc_set_segment_header(nullptr, TC_SEQ_FLAG_NO_SEG, 0);
    EXPECT_EQ(r, SDLP_ERROR_INVALID_PARAM);
}

TEST(TcSegmentHeader, FlagsMasked) {
    const uint8_t data[] = {0x01};
    sdlp_tc_frame_t frame;
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, sizeof(data)), SDLP_SUCCESS);
    /* Cast through the enum: only low 2 bits should be kept */
    int r = sdlp_tc_set_segment_header(&frame, static_cast<sdlp_tc_seq_flag_t>(0xFF), 0);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.segment_header.sequence_flags, 0x03U);
}

TEST(TcSegmentHeader, MapIdMasked) {
    const uint8_t data[] = {0x01};
    sdlp_tc_frame_t frame;
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, sizeof(data)), SDLP_SUCCESS);
    /* 0xFF: only lower 6 bits (0x3F) should be stored */
    int r = sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_NO_SEG, 0xFF);
    EXPECT_EQ(r, SDLP_SUCCESS);
    EXPECT_EQ(frame.segment_header.map_id, 0x3FU);
}

TEST(TcSegmentHeader, AllSequenceFlags) {
    const uint8_t data[] = {0x01};
    sdlp_tc_seq_flag_t flags[] = {TC_SEQ_FLAG_CONTINUE, TC_SEQ_FLAG_FIRST,
                                   TC_SEQ_FLAG_LAST, TC_SEQ_FLAG_NO_SEG};
    for (auto flag : flags) {
        sdlp_tc_frame_t frame;
        ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x001, 0, 0, data, sizeof(data)), SDLP_SUCCESS);
        EXPECT_EQ(sdlp_tc_set_segment_header(&frame, flag, 0), SDLP_SUCCESS);
        EXPECT_EQ(frame.segment_header.sequence_flags, (uint8_t)flag);
    }
}

TEST(TcSegmentHeader, EncodeDecodeRoundtrip) {
    const uint8_t payload[] = {0xAB, 0xCD};
    sdlp_tc_frame_t frame;
    ASSERT_EQ(sdlp_tc_create_frame(&frame, 0x123, 2, 5, payload, sizeof(payload)), SDLP_SUCCESS);
    ASSERT_EQ(sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_FIRST, 7), SDLP_SUCCESS);

    uint8_t buf[64];
    size_t encoded;
    ASSERT_EQ(sdlp_tc_encode_frame(&frame, buf, sizeof(buf), &encoded), SDLP_SUCCESS);
    /* With segment header the frame must be one byte larger */
    EXPECT_EQ(encoded, (size_t)(TC_PRIMARY_HEADER_SIZE + TC_SEGMENT_HEADER_SIZE +
                                sizeof(payload) + TC_FRAME_ERROR_CONTROL_SIZE));

    sdlp_tc_frame_t decoded;
    ASSERT_EQ(sdlp_tc_decode_frame(buf, encoded, &decoded), SDLP_SUCCESS);
    EXPECT_EQ(decoded.header.spacecraft_id, 0x123U);
    EXPECT_EQ(decoded.segment_header.sequence_flags, (uint8_t)TC_SEQ_FLAG_FIRST);
    EXPECT_EQ(decoded.segment_header.map_id, 7U);
    EXPECT_EQ(decoded.data_length, (uint16_t)sizeof(payload));
    EXPECT_EQ(memcmp(decoded.data, payload, sizeof(payload)), 0);
}
