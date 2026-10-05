/**
 * @file test_tm.c
 * @brief Unit tests for the TM Space Data Link Protocol frame handler.
 */
#include "cunit.h"
#include "sdlp_common.h"
#include "sdlp_tm.h"
#include "test_runners.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** @brief Scratch buffer size for rejected-encode checks: holds a frame just over the limit. */
#define TEST_TM_SCRATCH_SIZE (2 * TM_MAX_DATA_SIZE)

/** @brief Size of a frame with a full Data Field and neither Secondary Header nor OCF. */
#define TEST_TM_MAX_PLAIN_FRAME_SIZE                                                               \
    (TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE)

static int test_tm_create_frame_invalid_params(void)
{
    sdlp_tm_frame_t frame;
    uint8_t payload[1] = {0xAAu};

    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_create_frame(NULL, 1, 1, payload, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_create_frame(&frame, 1, 1, NULL, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tm_create_frame(&frame, 1, 1, payload, TM_MAX_DATA_SIZE + 1));

    return 0;
}

static int test_tm_encode_decode_roundtrip(void)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t decoded;
    const uint8_t payload[] = {0x10u, 0x20u, 0x30u, 0x40u, 0x50u};
    uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_create_frame(&frame, 0x7FFu, 0x1Fu, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    ASSERT_EQ_INT(TM_PRIMARY_HEADER_SIZE + (int)sizeof(payload) + TM_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(SDLP_VERSION, decoded.header.transfer_frame_version);
    ASSERT_EQ_INT((int)(0x7FFu & 0x3FFu), decoded.header.spacecraft_id);
    ASSERT_EQ_INT((int)(0x1Fu & 0x07u), decoded.header.virtual_channel_id);
    ASSERT_EQ_INT(frame.header.master_channel_frame_count,
                  decoded.header.master_channel_frame_count);
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));
    /* The default Data Field Status round-trips: Sync Flag = 0 with a '11' Segment
     * Length Identifier (CCSDS 132.0-B-3, 4.1.2.7.5.2). */
    ASSERT_EQ_INT(0, decoded.header.transfer_frame_data_field_status.sync_flag);
    ASSERT_EQ_INT(TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION,
                  decoded.header.transfer_frame_data_field_status.segment_length_id);

    return 0;
}

static int test_tm_data_field_status_codec(void)
{
    sdlp_tm_data_field_status_t status = {0};
    sdlp_tm_data_field_status_t parsed = {0};
    uint16_t raw;

    status.secondary_header_flag = 1;
    status.sync_flag = 0;
    status.packet_order_flag = 0;
    status.segment_length_id = TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION;
    status.first_header_pointer = 0x123u;

    /* MSB-first layout: shf<<15 | slid<<11 | fhp = 0x8000 | 0x1800 | 0x123. */
    raw = sdlp_tm_pack_data_field_status(&status);
    ASSERT_EQ_INT(0x9923u, raw);

    sdlp_tm_unpack_data_field_status(raw, &parsed);
    ASSERT_EQ_INT(1, parsed.secondary_header_flag);
    ASSERT_EQ_INT(0, parsed.sync_flag);
    ASSERT_EQ_INT(0, parsed.packet_order_flag);
    ASSERT_EQ_INT(TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION, parsed.segment_length_id);
    ASSERT_EQ_INT(0x123u, parsed.first_header_pointer);

    return 0;
}

static int test_tm_encode_buffer_too_small(void)
{
    sdlp_tm_frame_t frame;
    const uint8_t payload[] = {0x01u, 0x02u, 0x03u};
    uint8_t encoded[TM_PRIMARY_HEADER_SIZE + 2 + TM_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_create_frame(&frame, 2, 1, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_ERROR_BUFFER_TOO_SMALL,
                  sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    return 0;
}

static int test_tm_fecf_passthrough(void)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t decoded;
    const uint8_t payload[] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_create_frame(&frame, 3, 2, payload, (uint16_t)sizeof(payload)));
    frame.fecf = 0xABCDu;
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

    /* The FECF is serialized verbatim (big-endian) in the trailing two bytes. */
    ASSERT_EQ_INT(0xABu, encoded[encoded_size - 2]);
    ASSERT_EQ_INT(0xCDu, encoded[encoded_size - 1]);

    /* Decode no longer validates the FECF: it always succeeds and surfaces the
     * field as-is. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(0xABCDu, decoded.fecf);

    return 0;
}

static int test_tm_frame_counts_per_channel(void)
{
    sdlp_tm_frame_t f;
    const uint8_t payload[1] = {0xA5u};
    const uint16_t scid_a = 0x055u; /* SCIDs not used by other tests => fresh counters */
    const uint16_t scid_b = 0x056u;

    /* First frame on (SCID A, VC 0): both counts start at 0. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_a, 0, payload, 1));
    ASSERT_EQ_INT(0, f.header.master_channel_frame_count);
    ASSERT_EQ_INT(0, f.header.virtual_channel_frame_count);

    /* A different VC of the same Master Channel: the MC count advances, the new VC
     * count is independent and starts at 0. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_a, 1, payload, 1));
    ASSERT_EQ_INT(1, f.header.master_channel_frame_count);
    ASSERT_EQ_INT(0, f.header.virtual_channel_frame_count);

    /* Back to VC 0: the MC count keeps advancing, VC 0 resumes its own sequence. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_a, 0, payload, 1));
    ASSERT_EQ_INT(2, f.header.master_channel_frame_count);
    ASSERT_EQ_INT(1, f.header.virtual_channel_frame_count);

    /* A different Master Channel keeps entirely separate counts. */
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&f, scid_b, 0, payload, 1));
    ASSERT_EQ_INT(0, f.header.master_channel_frame_count);
    ASSERT_EQ_INT(0, f.header.virtual_channel_frame_count);

    return 0;
}

static int test_tm_secondary_header_roundtrip(void)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t decoded;
    const uint8_t payload[] = {0x10u, 0x20u, 0x30u};
    const uint8_t sh_data[] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
    uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE +
                    TM_SECONDARY_HEADER_MAX_DATA + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_create_frame(&frame, 0x100u, 1, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_set_secondary_header(&frame, sh_data, (uint8_t)sizeof(sh_data)));
    ASSERT_EQ_INT(1, frame.header.transfer_frame_data_field_status.secondary_header_flag);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    /* primary(6) + ID(1) + secondary data(4) + payload(3) + FECF(2) */
    ASSERT_EQ_INT(TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE + (int)sizeof(sh_data) +
                      (int)sizeof(payload) + TM_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(1, decoded.header.transfer_frame_data_field_status.secondary_header_flag);
    ASSERT_EQ_INT((int)sizeof(sh_data), decoded.secondary_header.length);
    ASSERT_EQ_MEM(sh_data, decoded.secondary_header.data, sizeof(sh_data));
    /* The Data Field must be recovered intact after the Secondary Header. */
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tm_set_secondary_header_invalid(void)
{
    sdlp_tm_frame_t frame;
    const uint8_t payload[1] = {0x01u};
    const uint8_t sh_data[1] = {0xFFu};

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&frame, 1, 0, payload, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_secondary_header(NULL, sh_data, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_secondary_header(&frame, NULL, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_secondary_header(&frame, sh_data, 0));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tm_set_secondary_header(&frame, sh_data, TM_SECONDARY_HEADER_MAX_DATA + 1));
    /* A rejected call must leave the Secondary Header Flag clear. */
    ASSERT_EQ_INT(0, frame.header.transfer_frame_data_field_status.secondary_header_flag);

    return 0;
}

static int test_tm_ocf_roundtrip(void)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t decoded;
    const uint8_t payload[] = {0x10u, 0x20u, 0x30u};
    const uint8_t ocf[TM_OCF_SIZE] = {0x01u, 0x02u, 0x03u, 0x04u};
    uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_OCF_SIZE +
                    TM_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_create_frame(&frame, 0x101u, 1, payload, (uint16_t)sizeof(payload)));
    /* No OCF by default. */
    ASSERT_EQ_INT(0, frame.header.ocf_flag);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_set_ocf(&frame, ocf));
    ASSERT_EQ_INT(1, frame.header.ocf_flag);

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    /* primary(6) + payload(3) + OCF(4) + FECF(2) */
    ASSERT_EQ_INT(TM_PRIMARY_HEADER_SIZE + (int)sizeof(payload) + TM_OCF_SIZE +
                      TM_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(1, decoded.header.ocf_flag);
    ASSERT_EQ_MEM(ocf, decoded.ocf, TM_OCF_SIZE);
    /* The OCF sits between the Data Field and the FECF; the data must be intact. */
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tm_secondary_header_and_ocf_roundtrip(void)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t decoded;
    const uint8_t payload[] = {0xAAu, 0xBBu};
    const uint8_t sh_data[] = {0x11u, 0x22u, 0x33u};
    const uint8_t ocf[TM_OCF_SIZE] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE +
                    TM_SECONDARY_HEADER_MAX_DATA + TM_MAX_DATA_SIZE + TM_OCF_SIZE +
                    TM_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size = 0;

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_create_frame(&frame, 0x102u, 2, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_set_secondary_header(&frame, sh_data, (uint8_t)sizeof(sh_data)));
    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_set_ocf(&frame, ocf));

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));
    /* primary(6) + ID(1) + secondary(3) + payload(2) + OCF(4) + FECF(2) */
    ASSERT_EQ_INT(TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE + (int)sizeof(sh_data) +
                      (int)sizeof(payload) + TM_OCF_SIZE + TM_FRAME_ERROR_CONTROL_SIZE,
                  (int)encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(1, decoded.header.transfer_frame_data_field_status.secondary_header_flag);
    ASSERT_EQ_INT(1, decoded.header.ocf_flag);
    ASSERT_EQ_MEM(sh_data, decoded.secondary_header.data, sizeof(sh_data));
    ASSERT_EQ_MEM(ocf, decoded.ocf, TM_OCF_SIZE);
    ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tm_set_ocf_invalid(void)
{
    sdlp_tm_frame_t frame;
    const uint8_t payload[1] = {0x01u};
    const uint8_t ocf[TM_OCF_SIZE] = {0};

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&frame, 1, 0, payload, 1));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_ocf(NULL, ocf));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_set_ocf(&frame, NULL));
    /* A rejected call must leave the OCF Flag clear. */
    ASSERT_EQ_INT(0, frame.header.ocf_flag);

    return 0;
}

static int test_tm_null_params(void)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t decoded;
    const uint8_t payload[1] = {0x01u};
    uint8_t buffer[16] = {0};
    size_t encoded_size = 0;

    /* The Data Field Status codec tolerates NULL. */
    ASSERT_EQ_INT(0, sdlp_tm_pack_data_field_status(NULL));
    sdlp_tm_unpack_data_field_status(0x1234u, NULL); /* must not dereference NULL */

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&frame, 1, 1, payload, 1));

    /* encode rejects each NULL argument. */
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tm_encode_frame(NULL, buffer, sizeof(buffer), &encoded_size));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tm_encode_frame(&frame, NULL, sizeof(buffer), &encoded_size));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tm_encode_frame(&frame, buffer, sizeof(buffer), NULL));

    /* decode rejects a NULL buffer, a NULL frame, and a buffer shorter than the header + FECF. */
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_decode_frame(NULL, sizeof(buffer), &decoded));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM, sdlp_tm_decode_frame(buffer, sizeof(buffer), NULL));
    ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
                  sdlp_tm_decode_frame(buffer,
                                       TM_PRIMARY_HEADER_SIZE + TM_FRAME_ERROR_CONTROL_SIZE - 1,
                                       &decoded));

    return 0;
}

static int test_tm_decode_malformed(void)
{
    sdlp_tm_frame_t decoded;

    /* Secondary Header Flag set (byte 4 bit 7), buffer too small for the Identification
     * Field plus a Data Field octet. */
    {
        uint8_t buf[9] = {0, 0, 0, 0, 0x80u};
        ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tm_decode_frame(buf, sizeof(buf), &decoded));
    }
    /* Secondary Header present but its Length field is zero (empty Data Field). */
    {
        uint8_t buf[10] = {0, 0, 0, 0, 0x80u};
        ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tm_decode_frame(buf, sizeof(buf), &decoded));
    }
    /* Secondary Header Length overruns the remaining buffer. */
    {
        uint8_t buf[10] = {0, 0, 0, 0, 0x80u, 0, 0x0Au};
        ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tm_decode_frame(buf, sizeof(buf), &decoded));
    }
    /* OCF Flag set (byte 1 bit 0), buffer too small to hold the 4-octet OCF. */
    {
        uint8_t buf[8] = {0, 0x01u};
        ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tm_decode_frame(buf, sizeof(buf), &decoded));
    }
    /* Data Field larger than TM_MAX_DATA_SIZE. */
    {
        static uint8_t
            big[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + 1 + TM_FRAME_ERROR_CONTROL_SIZE] = {0};
        ASSERT_EQ_INT(SDLP_ERROR_INVALID_FRAME, sdlp_tm_decode_frame(big, sizeof(big), &decoded));
    }

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
static int test_tm_expect_decode_rejected(const uint8_t *buffer,
                                          size_t buffer_size,
                                          sdlp_status_t expected)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t untouched;

    memset(&frame, 0xA5, sizeof(frame));
    memset(&untouched, 0xA5, sizeof(untouched));

    ASSERT_EQ_INT(expected, sdlp_tm_decode_frame(buffer, buffer_size, &frame));
    ASSERT_EQ_MEM(&untouched, &frame, sizeof(frame));

    return 0;
}

static int test_tm_decode_failure_leaves_frame_unchanged(void)
{
    const uint8_t too_short[TM_PRIMARY_HEADER_SIZE + TM_FRAME_ERROR_CONTROL_SIZE - 1] = {0};
    /* Secondary Header Flag set (byte 4 bit 7), no room for its Identification Field plus
     * one Data Field octet. */
    const uint8_t no_secondary_room[9] = {0, 0, 0, 0, 0x80u};
    /* Secondary Header Length of zero, i.e. an empty Data Field. */
    const uint8_t empty_secondary[10] = {0, 0, 0, 0, 0x80u};
    /* Secondary Header Length overruns the remaining buffer. */
    const uint8_t secondary_overrun[10] = {0, 0, 0, 0, 0x80u, 0, 0x0Au};
    /* OCF Flag set (byte 1 bit 0), no room for the 4-octet OCF. */
    const uint8_t no_ocf_room[8] = {0, 0x01u};
    /* Data Field one octet longer than TM_MAX_DATA_SIZE. */
    static const uint8_t oversized[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + 1 +
                                   TM_FRAME_ERROR_CONTROL_SIZE] = {0};

    ASSERT_EQ_INT(
        0,
        test_tm_expect_decode_rejected(NULL, sizeof(no_ocf_room), SDLP_ERROR_INVALID_PARAM));
    ASSERT_EQ_INT(
        0,
        test_tm_expect_decode_rejected(too_short, sizeof(too_short), SDLP_ERROR_INVALID_PARAM));
    ASSERT_EQ_INT(0,
                  test_tm_expect_decode_rejected(no_secondary_room,
                                                 sizeof(no_secondary_room),
                                                 SDLP_ERROR_INVALID_FRAME));
    ASSERT_EQ_INT(0,
                  test_tm_expect_decode_rejected(empty_secondary,
                                                 sizeof(empty_secondary),
                                                 SDLP_ERROR_INVALID_FRAME));
    ASSERT_EQ_INT(0,
                  test_tm_expect_decode_rejected(secondary_overrun,
                                                 sizeof(secondary_overrun),
                                                 SDLP_ERROR_INVALID_FRAME));
    ASSERT_EQ_INT(
        0,
        test_tm_expect_decode_rejected(no_ocf_room, sizeof(no_ocf_room), SDLP_ERROR_INVALID_FRAME));
    ASSERT_EQ_INT(
        0,
        test_tm_expect_decode_rejected(oversized, sizeof(oversized), SDLP_ERROR_INVALID_FRAME));

    return 0;
}

static int test_tm_decode_data_length_beyond_16_bits(void)
{
    /* A Data Field of 65546 octets reads as 10 once narrowed to 16 bits, so the decoder
     * must range-check the length before narrowing it. */
    const size_t data_length = (size_t)UINT16_MAX + 1u + 10u;
    const size_t buffer_size = TM_PRIMARY_HEADER_SIZE + data_length + TM_FRAME_ERROR_CONTROL_SIZE;
    uint8_t *buffer = calloc(buffer_size, 1);
    int result;

    ASSERT_TRUE(buffer);
    result = test_tm_expect_decode_rejected(buffer, buffer_size, SDLP_ERROR_INVALID_FRAME);
    free(buffer);
    ASSERT_EQ_INT(0, result);

    return 0;
}

/**
 * @brief Encode a frame the encoder must reject and check both outputs are untouched.
 *
 * The buffer and the encoded size are pre-filled with a sentinel pattern, so any write
 * by the encoder shows up as a difference.
 *
 * @param[in] frame       Frame to encode.
 * @param[in] buffer_size Capacity to offer the encoder, at most ::TEST_TM_SCRATCH_SIZE.
 * @param[in] expected    Status the encoder must return.
 * @return 0 if the encoder returned @p expected and left its outputs unchanged, 1 otherwise.
 */
static int test_tm_expect_encode_rejected(const sdlp_tm_frame_t *frame,
                                          size_t buffer_size,
                                          sdlp_status_t expected)
{
    uint8_t buffer[TEST_TM_SCRATCH_SIZE];
    uint8_t untouched[TEST_TM_SCRATCH_SIZE];
    size_t encoded_size = 0xA5A5u;

    memset(buffer, 0xA5, sizeof(buffer));
    memset(untouched, 0xA5, sizeof(untouched));

    ASSERT_EQ_INT(expected, sdlp_tm_encode_frame(frame, buffer, buffer_size, &encoded_size));
    ASSERT_EQ_INT(0xA5A5u, encoded_size);
    ASSERT_EQ_MEM(untouched, buffer, sizeof(buffer));

    return 0;
}

/**
 * @brief Round-trip a frame with the largest Data Field through an exact-size buffer.
 *
 * @param[out] encoded Buffer of exactly ::TEST_TM_MAX_PLAIN_FRAME_SIZE octets.
 * @return 0 if every check passed, 1 otherwise.
 */
static int test_tm_check_max_data_roundtrip(uint8_t *encoded)
{
    sdlp_tm_frame_t frame;
    sdlp_tm_frame_t decoded;
    uint8_t payload[TM_MAX_DATA_SIZE];
    size_t encoded_size = 0;

    for (size_t i = 0; i < sizeof(payload); i++)
    {
        payload[i] = (uint8_t)i;
    }

    ASSERT_EQ_INT(SDLP_SUCCESS,
                  sdlp_tm_create_frame(&frame, 1, 0, payload, (uint16_t)sizeof(payload)));
    ASSERT_EQ_INT(0,
                  test_tm_expect_encode_rejected(&frame,
                                                 TEST_TM_MAX_PLAIN_FRAME_SIZE - 1,
                                                 SDLP_ERROR_BUFFER_TOO_SMALL));

    ASSERT_EQ_INT(
        SDLP_SUCCESS,
        sdlp_tm_encode_frame(&frame, encoded, TEST_TM_MAX_PLAIN_FRAME_SIZE, &encoded_size));
    ASSERT_EQ_INT(TEST_TM_MAX_PLAIN_FRAME_SIZE, encoded_size);

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
    ASSERT_EQ_INT(TM_MAX_DATA_SIZE, decoded.data_length);
    ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

    return 0;
}

static int test_tm_encode_max_data_exact_buffer(void)
{
    uint8_t *encoded = malloc(TEST_TM_MAX_PLAIN_FRAME_SIZE);
    int result;

    ASSERT_TRUE(encoded);
    result = test_tm_check_max_data_roundtrip(encoded);
    free(encoded);
    ASSERT_EQ_INT(0, result);

    return 0;
}

static int test_tm_encode_rejects_oversized_data(void)
{
    sdlp_tm_frame_t frame;
    const uint8_t payload[1] = {0x01u};

    ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_create_frame(&frame, 1, 0, payload, 1));

    /* One octet more than the Data Field array holds. */
    frame.data_length = TM_MAX_DATA_SIZE + 1;
    ASSERT_EQ_INT(
        0,
        test_tm_expect_encode_rejected(&frame, TEST_TM_SCRATCH_SIZE, SDLP_ERROR_INVALID_PARAM));

    /* A length far beyond the Data Field array must not be read out of the frame. */
    frame.data_length = UINT16_MAX;
    ASSERT_EQ_INT(
        0,
        test_tm_expect_encode_rejected(&frame, TEST_TM_SCRATCH_SIZE, SDLP_ERROR_INVALID_PARAM));

    return 0;
}

test_result_t test_tm_run_all(void)
{
    test_result_t result;

    RUN_TEST(test_tm_create_frame_invalid_params);
    RUN_TEST(test_tm_encode_decode_roundtrip);
    RUN_TEST(test_tm_data_field_status_codec);
    RUN_TEST(test_tm_encode_buffer_too_small);
    RUN_TEST(test_tm_fecf_passthrough);
    RUN_TEST(test_tm_frame_counts_per_channel);
    RUN_TEST(test_tm_secondary_header_roundtrip);
    RUN_TEST(test_tm_set_secondary_header_invalid);
    RUN_TEST(test_tm_ocf_roundtrip);
    RUN_TEST(test_tm_secondary_header_and_ocf_roundtrip);
    RUN_TEST(test_tm_set_ocf_invalid);
    RUN_TEST(test_tm_null_params);
    RUN_TEST(test_tm_decode_malformed);
    RUN_TEST(test_tm_decode_failure_leaves_frame_unchanged);
    RUN_TEST(test_tm_decode_data_length_beyond_16_bits);
    RUN_TEST(test_tm_encode_max_data_exact_buffer);
    RUN_TEST(test_tm_encode_rejects_oversized_data);

    /* cunit's counters have internal linkage, so this translation unit tallies
     * only its own tests. */
    result.total = cunit_total_tests;
    result.passed = cunit_total_tests - cunit_overall_failures;
    return result;
}
