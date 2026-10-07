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
#include <stdlib.h>
#include <string.h>

/**
 * @brief Octets the Segment Header adds to a Type-D frame in this build configuration.
 *
 * The suite is built twice, without and with TC_SEGMENT_HEADER_ENABLED, so every size that
 * depends on the Segment Header is written in terms of this value.
 */
#ifdef TC_SEGMENT_HEADER_ENABLED
#    define TEST_TC_SEGMENT_OCTETS TC_SEGMENT_HEADER_SIZE
#else
#    define TEST_TC_SEGMENT_OCTETS 0
#endif

/** @brief Scratch buffer size for rejected-encode checks: holds a frame just over the limit. */
#define TEST_TC_SCRATCH_SIZE (2 * TC_MAX_FRAME_SIZE)

/**
 * @brief Build a frame the library must reject and check the output frame is untouched.
 *
 * @param[in] data        Data Field content (may be NULL).
 * @param[in] data_length Data Field length in octets.
 * @return 0 if sdlp_tc_create_frame() returned ::SDLP_ERROR_INVALID_PARAM and left the frame
 *         unchanged, 1 otherwise.
 */
static int test_tc_expect_create_rejected(const uint8_t *data, uint16_t data_length)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t untouched;

    memset(&frame, 0xA5, sizeof(frame));
    memset(&untouched, 0xA5, sizeof(untouched));

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_create_frame(&frame, 1, 1, 1, data, data_length));
    ASSERT_EQ_MEM(&untouched, &frame, sizeof(frame));

    return 0;
}

static int test_tc_create_frame_invalid_params(void)
{
    static const uint8_t payload[TC_MAX_DATA_SIZE + 1] = {0x55u};
    /* The largest Data Field is the array, less the Segment Header octet when compiled in. */
    const uint16_t max_data = TC_MAX_DATA_SIZE - TEST_TC_SEGMENT_OCTETS;

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_create_frame(NULL, 1, 1, 1, payload, 1));
    ASSERT_EQ_INT(0, test_tc_expect_create_rejected(NULL, 1));
    ASSERT_EQ_INT(0, test_tc_expect_create_rejected(payload, 0));
    /* The first rejected length is one octet past the largest Data Field. */
    ASSERT_EQ_INT(0, test_tc_expect_create_rejected(payload, (uint16_t)(max_data + 1)));
    ASSERT_EQ_INT(0, test_tc_expect_create_rejected(payload, TC_MAX_DATA_SIZE + 1));

    return 0;
}

static int test_tc_encode_decode_roundtrip(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0x01u, 0x23u, 0x45u, 0x67u};
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;
    const int seg = TEST_TC_SEGMENT_OCTETS;

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
    /* One octet short of the encoded frame. */
    uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TEST_TC_SEGMENT_OCTETS + sizeof(payload) - 1 +
                    TC_FRAME_ERROR_CONTROL_SIZE];
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

    /* Decode does not validate the FECF: it surfaces the field as-is. */
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
    sdlp_tc_frame_t before;
    const uint8_t payload[1] = {0x01u};

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 1));
    memcpy(&before, &frame, sizeof(frame));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tc_set_frame_type(NULL, SDLP_TC_FRAME_TYPE_BD));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_set_frame_type(&frame, (sdlp_tc_frame_type_t)99));
    /* A rejected type leaves the whole frame as it was, flags and Frame Length included. */
    ASSERT_EQ_MEM(&before, &frame, sizeof(frame));

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

#ifdef TC_SEGMENT_HEADER_ENABLED
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
    /* On the wire: Sequence Flags '01' in the top two bits, then the MAP ID. */
    ASSERT_EQ_INT(0x6Fu, encoded[TC_PRIMARY_HEADER_SIZE]);
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
#endif /* TC_SEGMENT_HEADER_ENABLED */

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
    uint8_t *exact = NULL;
    sdlp_status_t status;

    memset(&frame, 0xA5, sizeof(frame));
    memset(&untouched, 0xA5, sizeof(untouched));

    /* The decoder reads a heap copy of exactly buffer_size octets, so ASan reports any
     * read past the end of the input. */
    if (buffer)
    {
        exact = malloc((buffer_size > 0u) ? buffer_size : 1u);
        ASSERT_TRUE(exact);
        memcpy(exact, buffer, buffer_size);
    }
    status = sdlp_tc_decode_frame(exact, buffer_size, &frame);
    free(exact);

    ASSERT_EQ_INT(expected, status);
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
#ifdef TC_SEGMENT_HEADER_ENABLED
    /* A consistent 7-octet Type-D frame has no room for the Segment Header and the FECF. */
    const uint8_t no_segment_room[7] = {0, 0, 0, 6};
#endif

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
#ifdef TC_SEGMENT_HEADER_ENABLED
    ASSERT_EQ_INT(0,
                  test_tc_expect_decode_rejected(no_segment_room,
                                                 sizeof(no_segment_room),
                                                 SDLP_ERROR_INVALID_FRAME));
#endif

    return 0;
}

/**
 * @brief Encode a frame the encoder must reject and check both outputs are untouched.
 *
 * The buffer and the encoded size are pre-filled with a sentinel pattern, so any write
 * by the encoder shows up as a difference.
 *
 * @param[in] frame       Frame to encode.
 * @param[in] buffer_size Capacity to offer the encoder, at most ::TEST_TC_SCRATCH_SIZE.
 * @param[in] expected    Status the encoder must return.
 * @return 0 if the encoder returned @p expected and left its outputs unchanged, 1 otherwise.
 */
static int test_tc_expect_encode_rejected(const sdlp_tc_frame_t *frame,
                                          size_t buffer_size,
                                          sdlp_status_t expected)
{
    uint8_t buffer[TEST_TC_SCRATCH_SIZE];
    uint8_t untouched[TEST_TC_SCRATCH_SIZE];
    size_t encoded_size = 0xA5A5u;

    memset(buffer, 0xA5, sizeof(buffer));
    memset(untouched, 0xA5, sizeof(untouched));

    ASSERT_EQ_INT(expected, sdlp_tc_encode_frame(frame, buffer, buffer_size, &encoded_size));
    ASSERT_EQ_INT(0xA5A5u, encoded_size);
    ASSERT_EQ_MEM(untouched, buffer, sizeof(buffer));

    return 0;
}

/**
 * @brief Round-trip the largest legal Type-D frame through an exact-size buffer.
 *
 * @param[out] encoded Buffer of exactly ::TC_MAX_FRAME_SIZE octets.
 * @return 0 if every check passed, 1 otherwise.
 */
static int test_tc_check_max_frame_roundtrip(uint8_t *encoded)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    uint8_t payload[TC_MAX_DATA_SIZE - TEST_TC_SEGMENT_OCTETS];
    size_t encoded_size = 0;

    for (size_t i = 0; i < sizeof(payload); i++)
    {
        payload[i] = (uint8_t)i;
    }

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x3FFu, 0x3Fu, 0xFFu, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(
        0,
        test_tc_expect_encode_rejected(&frame, TC_MAX_FRAME_SIZE - 1, SDLP_ERROR_BUFFER_TOO_SMALL));

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, TC_MAX_FRAME_SIZE, &encoded_size));
    ASSERT_EQ_INT(TC_MAX_FRAME_SIZE, encoded_size);
    /* Frame Length = 1023 sets all ten bits: the low two of octet 2 and all of octet 3. */
    ASSERT_EQ_INT(0x03u, encoded[2] & 0x03u);
    ASSERT_EQ_INT(0xFFu, encoded[3]);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tc_encode_max_frame_exact_buffer(void)
{
    uint8_t *encoded = malloc(TC_MAX_FRAME_SIZE);
    int result;

    ASSERT_TRUE(encoded);
    result = test_tc_check_max_frame_roundtrip(encoded);
    free(encoded);
    ASSERT_EQ_INT(0, result);

    return 0;
}

static int test_tc_encode_rejects_oversized_frame(void)
{
    sdlp_tc_frame_t frame;
    const uint8_t payload[1] = {0x01u};
    uint8_t encoded[TC_MAX_FRAME_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 1));

    /* Type-D: one octet more than the largest Data Field, which is the array less the
     * Segment Header octet when that is compiled in. The frame would be 1025 octets, one
     * more than the 10-bit Frame Length can express. */
    frame.data_length = TC_MAX_DATA_SIZE - TEST_TC_SEGMENT_OCTETS + 1;
    ASSERT_EQ_INT(
        0,
        test_tc_expect_encode_rejected(&frame, TEST_TC_SCRATCH_SIZE, SDLP_ERROR_INVALID_PARAM));

    /* A length far beyond the Data Field array must not be read out of the frame. */
    frame.data_length = UINT16_MAX;
    ASSERT_EQ_INT(
        0,
        test_tc_expect_encode_rejected(&frame, TEST_TC_SCRATCH_SIZE, SDLP_ERROR_INVALID_PARAM));

    /* Type-BC carries no Segment Header: a full Data Field array is exactly the largest
     * frame, and one octet more is rejected. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_frame_type(&frame, SDLP_TC_FRAME_TYPE_BC));
    frame.data_length = TC_MAX_DATA_SIZE;
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT(TC_MAX_FRAME_SIZE, encoded_size);

    frame.data_length = TC_MAX_DATA_SIZE + 1;
    ASSERT_EQ_INT(
        0,
        test_tc_expect_encode_rejected(&frame, TEST_TC_SCRATCH_SIZE, SDLP_ERROR_INVALID_PARAM));

    return 0;
}

/**
 * @brief A complete Type-BD frame with a distinct value in every field, as sent on the wire.
 */
static const uint8_t test_tc_golden[] = {
    0x22u, /* Version 00, Bypass 1, Control Command 0, Reserved 00, Spacecraft ID bits 9-8 */
    0xA5u, /* Spacecraft ID bits 7-0: the ID is 0x2A5 */
    0x54u, /* Virtual Channel ID 0x15, Frame Length bits 9-8 */
    TC_PRIMARY_HEADER_SIZE + TEST_TC_SEGMENT_OCTETS + 2 + TC_FRAME_ERROR_CONTROL_SIZE - 1,
    0x9Cu, /* Frame Sequence Number */
#ifdef TC_SEGMENT_HEADER_ENABLED
    0xAFu, /* Sequence Flags '10' (last portion), MAP ID 0x2F */
#endif
    0xD1u,
    0xD2u, /* Data Field */
    0xFEu,
    0xC5u /* FECF */
};

static int test_tc_golden_frame_encode(void)
{
    sdlp_tc_frame_t frame;
    const uint8_t payload[] = {0xD1u, 0xD2u};
    uint8_t encoded[sizeof(test_tc_golden)];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tc_create_frame(&frame, 0x2A5u, 0x15u, 0x9Cu, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_frame_type(&frame, SDLP_TC_FRAME_TYPE_BD));
#ifdef TC_SEGMENT_HEADER_ENABLED
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_LAST, 0x2Fu));
#endif
    frame.fecf = 0xFEC5u;

    /* The buffer is exactly as long as the frame, and every octet is compared. */
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT((int)sizeof(test_tc_golden), (int)encoded_size);
    ASSERT_EQ_MEM(test_tc_golden, encoded, sizeof(test_tc_golden));

    return 0;
}

static int test_tc_golden_frame_decode(void)
{
    sdlp_tc_frame_t decoded;
    const uint8_t payload[] = {0xD1u, 0xD2u};

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_decode_frame(test_tc_golden, sizeof(test_tc_golden), &decoded));
    ASSERT_EQ_INT(0, decoded.header.transfer_frame_version);
    ASSERT_EQ_INT(1, decoded.header.bypass_flag);
    ASSERT_EQ_INT(0, decoded.header.control_command_flag);
    ASSERT_EQ_INT(0, decoded.header.reserved);
    ASSERT_EQ_INT(0x2A5u, decoded.header.spacecraft_id);
    ASSERT_EQ_INT(0x15u, decoded.header.virtual_channel_id);
    ASSERT_EQ_INT((int)sizeof(test_tc_golden) - 1, decoded.header.frame_length);
    ASSERT_EQ_INT(0x9Cu, decoded.header.frame_sequence_number);
#ifdef TC_SEGMENT_HEADER_ENABLED
    ASSERT_EQ_INT(TC_SEQ_FLAG_LAST, decoded.segment_header.sequence_flags);
    ASSERT_EQ_INT(0x2Fu, decoded.segment_header.map_id);
#endif
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));
    ASSERT_EQ_INT(0xFEC5u, decoded.fecf);

    return 0;
}

static int test_tc_primary_header_wire_limits(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t decoded;
    const uint8_t payload[1] = {0x00u};
    uint8_t encoded[TC_MAX_FRAME_SIZE];
    size_t encoded_size = 0;
    /* Every field the caller sets is zero; only the Frame Length (total octets - 1) is not. */
    const uint8_t all_zero[TC_PRIMARY_HEADER_SIZE] = {
        0x00u,
        0x00u,
        0x00u,
        TC_PRIMARY_HEADER_SIZE + TEST_TC_SEGMENT_OCTETS + 1 + TC_FRAME_ERROR_CONTROL_SIZE - 1,
        0x00u};
    const uint8_t all_max[TC_PRIMARY_HEADER_SIZE] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 0, 0, 0, payload, 1));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_MEM(all_zero, encoded, sizeof(all_zero));

    /* Every field at its maximum. A Type-BC frame carries no Segment Header, so a full Data
     * Field array makes the largest frame and the Frame Length reaches 1023 as well. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 0x3FFu, 0x3Fu, 0xFFu, payload, 1));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_frame_type(&frame, SDLP_TC_FRAME_TYPE_BC));
    frame.header.transfer_frame_version = 0x03u;
    frame.header.reserved = 0x03u;
    frame.data_length = TC_MAX_DATA_SIZE;
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_MEM(all_max, encoded, sizeof(all_max));

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(0x03u, decoded.header.transfer_frame_version);
    ASSERT_EQ_INT(1, decoded.header.bypass_flag);
    ASSERT_EQ_INT(1, decoded.header.control_command_flag);
    ASSERT_EQ_INT(0x03u, decoded.header.reserved);
    ASSERT_EQ_INT(0x3FFu, decoded.header.spacecraft_id);
    ASSERT_EQ_INT(0x3Fu, decoded.header.virtual_channel_id);
    ASSERT_EQ_INT(TC_MAX_FRAME_SIZE - 1, decoded.header.frame_length);
    ASSERT_EQ_INT(0xFFu, decoded.header.frame_sequence_number);

    return 0;
}

#ifdef TC_SEGMENT_HEADER_ENABLED
static int test_tc_segment_header_wire_limits(void)
{
    sdlp_tc_frame_t frame;
    const uint8_t payload[1] = {0x00u};
    uint8_t
        encoded[TC_PRIMARY_HEADER_SIZE + TC_SEGMENT_HEADER_SIZE + 1 + TC_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 1));

    /* Both fields zero, then both at their maximum: the octet right after the primary header. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_CONTINUE, 0));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT(0x00u, encoded[TC_PRIMARY_HEADER_SIZE]);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_NO_SEG, 0x3Fu));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    ASSERT_EQ_INT(0xFFu, encoded[TC_PRIMARY_HEADER_SIZE]);

    return 0;
}

static int test_tc_segment_header_rejected_on_control_command(void)
{
    sdlp_tc_frame_t frame;
    sdlp_tc_frame_t before;
    const uint8_t payload[1] = {0x01u};

    /* A frame carrying a Control Command must not have a Segment Header (4.1.3.2.2.1.3):
     * the call is refused and the frame is left as it was. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_unlock_frame(&frame, 1, 1));
    memcpy(&before, &frame, sizeof(frame));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_NO_SEG, 0x2Fu));
    ASSERT_EQ_MEM(&before, &frame, sizeof(frame));

    /* The other Bypass frame type, Type-BD, carries data and still accepts one. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_create_frame(&frame, 1, 1, 1, payload, 1));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_frame_type(&frame, SDLP_TC_FRAME_TYPE_BD));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_NO_SEG, 0x2Fu));
    ASSERT_EQ_INT(0x2Fu, frame.segment_header.map_id);

    return 0;
}
#endif /* TC_SEGMENT_HEADER_ENABLED */

static int test_tc_decode_short_inputs(void)
{
    static const uint8_t zeros[TC_PRIMARY_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE] = {0};
    /* The shortest frame: a Type-BC header (so no Segment Header), no data, and the FECF. */
    const uint8_t shortest[TC_PRIMARY_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE] =
        {0x30u, 0x00u, 0x00u, TC_PRIMARY_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE - 1};
    sdlp_tc_frame_t decoded;
    uint8_t *exact;
    sdlp_status_t status;

    /* Every length below a primary header plus the FECF is rejected. */
    for (size_t length = 0; length < sizeof(zeros); length++)
    {
        ASSERT_EQ_INT(0, test_tc_expect_decode_rejected(zeros, length, SDLP_ERROR_INVALID_PARAM));
    }

    /* Exactly that length is the first one accepted, read from an exact-size heap buffer. */
    exact = malloc(sizeof(shortest));
    ASSERT_TRUE(exact);
    memcpy(exact, shortest, sizeof(shortest));
    status = sdlp_tc_decode_frame(exact, sizeof(shortest), &decoded);
    free(exact);
    ASSERT_EQ_INT(SDLP_SUCCESS, status);
    ASSERT_EQ_INT(1, decoded.header.control_command_flag);
    ASSERT_EQ_INT(0, decoded.data_length);

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
#ifdef TC_SEGMENT_HEADER_ENABLED
    RUN_TEST(test_tc_segment_header_roundtrip);
    RUN_TEST(test_tc_decode_segment_too_small);
#endif
    RUN_TEST(test_tc_decode_failure_leaves_frame_unchanged);
    RUN_TEST(test_tc_encode_max_frame_exact_buffer);
    RUN_TEST(test_tc_encode_rejects_oversized_frame);
    RUN_TEST(test_tc_golden_frame_encode);
    RUN_TEST(test_tc_golden_frame_decode);
    RUN_TEST(test_tc_primary_header_wire_limits);
#ifdef TC_SEGMENT_HEADER_ENABLED
    RUN_TEST(test_tc_segment_header_wire_limits);
    RUN_TEST(test_tc_segment_header_rejected_on_control_command);
#endif
    RUN_TEST(test_tc_decode_short_inputs);

    /* cunit's counters have internal linkage, so this translation unit tallies
     * only its own tests. */
    result.total = cunit_total_tests;
    result.passed = cunit_total_tests - cunit_overall_failures;
    return result;
}
