/**
 * @file test_tc.c
 * @brief Unit tests for the TC Space Data Link Protocol frame handler.
 */
#include "cunit.h"
#include "sdlp_common.h"
#include "sdlp_tc.h"
#include "test_runners.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* The TC unit tests always exercise the segment-header configuration. */
#ifndef TC_SEGMENT_HEADER_ENABLED
#error "test_tc.c must be built with -DTC_SEGMENT_HEADER_ENABLED"
#endif

static int test_tc_create_frame_invalid_params(void)
{
    sdlp_tc_frame_t frame;
    uint8_t payload[1] = {0x55u};

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_frame(NULL, 1, 1, 1, payload, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_frame(&frame, 1, 1, 1, NULL, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_create_frame(&frame, 1, 1, 1, payload, TC_MAX_DATA_SIZE + 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 0));

    return 0;
}

static int test_tc_encode_decode_roundtrip(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x01u, 0x23u, 0x45u, 0x67u};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;
    const int seg = TC_SEGMENT_HEADER_SIZE; /* a Type-D frame carries a Segment Header */

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x7FFu, 0x7Fu, 0x9Au, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + seg + (int)sizeof(payload) + TC_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(SDLP_VERSION, decoded.header.transfer_frame_version);
    ASSERT_EQ_INT((int)(0x7FFu & 0x3FFu), decoded.header.spacecraft_id);
    ASSERT_EQ_INT((int)(0x7Fu & 0x3Fu), decoded.header.virtual_channel_id);
    ASSERT_EQ_INT(0x9Au, decoded.header.frame_sequence_number);
    /* Frame Length = total octets in the frame - 1 (CCSDS 232.0-B-4, 4.1.2.7.2). */
    ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + seg + (int)sizeof(payload) +
                      TC_FRAME_ERROR_CONTROL_SIZE - 1,
                  decoded.header.frame_length);
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tc_encode_buffer_too_small(void)
{
    sdlp_tc_frame_t frame;
    const uint8_t payload[] = {0xABu, 0xCDu, 0xEFu};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + 2 + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_create_frame(&frame, 5, 3, 7, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_ERROR_BUFFER_TOO_SMALL,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    return 0;
}

static int test_tc_fecf_passthrough(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x12u, 0x05u, 0x42u, payload, (uint16_t)sizeof(payload)));
    frame.fecf = 0x1234u;
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    /* The FECF is serialized verbatim (big-endian) in the trailing two bytes. */
    ASSERT_EQ_INT(0x12u, encoded[encoded_size - 2]);
    ASSERT_EQ_INT(0x34u, encoded[encoded_size - 1]);

    /* Decode no longer validates the FECF: it always succeeds and surfaces the
     * field as-is. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(0x1234u, decoded.fecf);

    return 0;
}

static int test_tc_decode_invalid_frame_length(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x01u, 0x02u, 0x03u, 0x04u};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x21u, 0x06u, 0x07u, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    /* Corrupt the Frame Length low byte so it no longer matches the octet count. */
    encoded[3] ^= 0x01u;

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));

    return 0;
}

static int test_tc_frame_type_bd_roundtrip(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x5Au, 0xA5u};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x44u, 0x02u, 0x10u, payload, (uint16_t)sizeof(payload)));
    /* Default is Type-AD. */
    ASSERT_EQ_INT(0, frame.header.bypass_flag);
    ASSERT_EQ_INT(0, frame.header.control_command_flag);

    /* Explicitly selecting Type-AD keeps both flags clear. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_frame_type(&frame, SDLP_TC_FRAME_TYPE_AD));
    ASSERT_EQ_INT(0, frame.header.bypass_flag);
    ASSERT_EQ_INT(0, frame.header.control_command_flag);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_frame_type(&frame, SDLP_TC_FRAME_TYPE_BD));
    ASSERT_EQ_INT(1, frame.header.bypass_flag);
    ASSERT_EQ_INT(0, frame.header.control_command_flag);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(1, decoded.header.bypass_flag);
    ASSERT_EQ_INT(0, decoded.header.control_command_flag);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tc_set_frame_type_invalid(void)
{
    sdlp_tc_frame_t frame;
    const uint8_t payload[1] = {0x01u};

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_set_frame_type(NULL, SDLP_TC_FRAME_TYPE_BD));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_set_frame_type(&frame, (sdlp_tc_frame_type_t)99));

    return 0;
}

static int test_tc_unlock_command(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_unlock_frame(&frame, 0x30u, 0x01u));
    /* Type-BC: Bypass=1, Control Command=1 (table 4-1). */
    ASSERT_EQ_INT(1, frame.header.bypass_flag);
    ASSERT_EQ_INT(1, frame.header.control_command_flag);
    /* Frame Sequence Number is 'all zeroes' for Type-B frames. */
    ASSERT_EQ_INT(0, frame.header.frame_sequence_number);
    /* Unlock: a single 'all zeroes' octet (4.1.3.3.2). */
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK_LENGTH, frame.data_length);
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK, frame.data[0]);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    /* Type-BC frames never carry a Segment Header (4.1.3.2.2.1.3). */
    ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + (int)TC_CONTROL_CMD_UNLOCK_LENGTH +
                      TC_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(1, decoded.header.bypass_flag);
    ASSERT_EQ_INT(1, decoded.header.control_command_flag);
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK_LENGTH, decoded.data_length);
    ASSERT_EQ_INT(TC_CONTROL_CMD_UNLOCK, decoded.data[0]);

    return 0;
}

static int test_tc_set_vr_command(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_set_vr_frame(&frame, 0x30u, 0x01u, 0x7Cu));
    ASSERT_EQ_INT(1, frame.header.bypass_flag);
    ASSERT_EQ_INT(1, frame.header.control_command_flag);
    ASSERT_EQ_INT(0, frame.header.frame_sequence_number);
    /* Set V(R): '10000010 00000000 XXXXXXXX' (4.1.3.3.3). */
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_LENGTH, frame.data_length);
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_OCTET0, frame.data[0]);
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_OCTET1, frame.data[1]);
    ASSERT_EQ_INT(0x7Cu, frame.data[2]);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(TC_CONTROL_CMD_SET_VR_LENGTH, decoded.data_length);
    ASSERT_EQ_INT(0x7Cu, decoded.data[2]);

    return 0;
}

static int test_tc_decode_reserved_frame_type(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x01u, 0x02u};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x11u, 0x01u, 0x01u, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    /* Force Bypass=0, Control Command=1: reserved for future application (table 4-1). */
    encoded[0] = (uint8_t)((encoded[0] & ~0x30u) | 0x10u);

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));

    return 0;
}

static int test_tc_null_params(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[1] = {0x01u};
    uint8_t buffer[16] = {0};
    size_t encoded_size = 0;

    /* The control-command builders reject a NULL frame (create fails, error propagates). */
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_unlock_frame(NULL, 1, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_set_vr_frame(NULL, 1, 1, 0));

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 1));

    /* encode rejects each NULL argument. */
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_encode_frame(NULL, buffer, sizeof(buffer), &encoded_size));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_encode_frame(&frame, NULL, sizeof(buffer), &encoded_size));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_encode_frame(&frame, buffer, sizeof(buffer), NULL));

    /* decode rejects a NULL buffer, a NULL frame, and a buffer shorter than the header + FECF. */
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_decode_frame(NULL, sizeof(buffer), &decoded));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_decode_frame(buffer, sizeof(buffer), NULL));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_decode_frame(buffer,
                                       TC_PRIMARY_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE - 1,
                                       &decoded));

    return 0;
}

static int test_tc_segment_header_roundtrip(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0xC0u, 0xDEu};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_SEGMENT_HEADER_SIZE + TC_MAX_DATA_SIZE +
                    TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x2Au, 0x03u, 0x08u, payload, (uint16_t)sizeof(payload)));

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_set_segment_header(NULL, TC_SEQ_FLAG_NO_SEG, 0));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_FIRST, 0x2Fu));
    ASSERT_EQ_INT(TC_SEQ_FLAG_FIRST, frame.segment_header.sequence_flags);
    ASSERT_EQ_INT(0x2Fu, frame.segment_header.map_id);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    /* primary(5) + segment(1) + payload(2) + FECF(2) */
    ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + TC_SEGMENT_HEADER_SIZE + (int)sizeof(payload) +
                      TC_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(TC_SEQ_FLAG_FIRST, decoded.segment_header.sequence_flags);
    ASSERT_EQ_INT(0x2Fu, decoded.segment_header.map_id);
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tc_decode_segment_too_small(void)
{
    sdlp_tc_frame_t decoded;

    /* Type-D frame (Control Command = 0) with Frame Length = 6 (buffer = 7 octets), which
     * is too small to hold the 1-octet Segment Header plus the 2-octet FECF. */
    uint8_t buf[7] = {0, 0, 0, 6};

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tc_decode_frame(buf, sizeof(buf), &decoded));

    return 0;
}

/**
 * @brief Decode a buffer the decoder must reject and check the output frame is untouched.
 *
 * The frame is pre-filled with a sentinel pattern, so any write by the decoder shows up
 * as a difference, whichever field it lands in.
 *
 * @param[in] buffer      Wire buffer to decode (may be NULL).
 * @param[in] buffer_size Buffer length in octets.
 * @param[in] expected    Status the decoder must return.
 * @return 0 if the decoder returned @p expected and left the frame unchanged, 1 otherwise.
 */
static int test_tc_expect_decode_rejected(const uint8_t *buffer,
                                          size_t buffer_size,
                                          sdlp_status_t expected)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t untouched;

    memset(&frame, 0xA5, sizeof(frame));
    memset(&untouched, 0xA5, sizeof(untouched));

    ASSERT_EQ_INT(expected, sdlp_tc_decode_frame(buffer, buffer_size, &frame));
    ASSERT_EQ_MEM(&untouched, &frame, sizeof(frame));

    return 0;
}

static int test_tc_decode_failure_leaves_frame_unchanged(void)
{
    const uint8_t too_short[TC_PRIMARY_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE - 1] = {0};
    /* Bypass=0, Control Command=1 is reserved (table 4-1); the Frame Length is consistent. */
    const uint8_t reserved_type[8] = {0x10u, 0, 0, 7};
    /* Frame Length = 6 announces 7 octets, but 8 are present. */
    const uint8_t wrong_length[8] = {0, 0, 0, 6};
    /* A consistent 7-octet Type-D frame has no room for the Segment Header and the FECF. */
    const uint8_t no_segment_room[7] = {0, 0, 0, 6};

    ASSERT_EQ_INT(
        0,
        test_tc_expect_decode_rejected(NULL, sizeof(wrong_length), SDLP_ERROR_INVALID_PARAM));
    ASSERT_EQ_INT(
        0,
        test_tc_expect_decode_rejected(too_short, sizeof(too_short), SDLP_ERROR_INVALID_PARAM));
    ASSERT_EQ_INT(0,
                  test_tc_expect_decode_rejected(reserved_type,
                                                 sizeof(reserved_type),
                                                 SDLP_ERROR_INVALID_FRAME));
    ASSERT_EQ_INT(0,
                  test_tc_expect_decode_rejected(wrong_length,
                                                 sizeof(wrong_length),
                                                 SDLP_ERROR_INVALID_FRAME));
    ASSERT_EQ_INT(0,
                  test_tc_expect_decode_rejected(no_segment_room,
                                                 sizeof(no_segment_room),
                                                 SDLP_ERROR_INVALID_FRAME));

    return 0;
}

test_result_t test_tc_run_all(void)
{
    test_result_t result;

    RUN_TEST(test_tc_create_frame_invalid_params);
    RUN_TEST(test_tc_null_params);
    RUN_TEST(test_tc_encode_decode_roundtrip);
    RUN_TEST(test_tc_encode_buffer_too_small);
    RUN_TEST(test_tc_fecf_passthrough);
    RUN_TEST(test_tc_decode_invalid_frame_length);
    RUN_TEST(test_tc_frame_type_bd_roundtrip);
    RUN_TEST(test_tc_set_frame_type_invalid);
    RUN_TEST(test_tc_unlock_command);
    RUN_TEST(test_tc_set_vr_command);
    RUN_TEST(test_tc_decode_reserved_frame_type);
    RUN_TEST(test_tc_segment_header_roundtrip);
    RUN_TEST(test_tc_decode_segment_too_small);
    RUN_TEST(test_tc_decode_failure_leaves_frame_unchanged);

    /* cunit's counters have internal linkage, so this translation unit tallies
     * only its own tests. */
    result.total = cunit_total_tests;
    result.passed = cunit_total_tests - cunit_overall_failures;
    return result;
}
