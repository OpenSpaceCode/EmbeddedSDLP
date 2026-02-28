#include "cunit.h"

#include <stddef.h>
#include <stdint.h>

#include "sdlp_common.h"
#include "sdlp_tc.h"
#include "sdlp_tm.h"

static int test_crc16_known_vector(void) {
	const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
	ASSERT_EQ_INT(0x29B1, sdlp_crc16(data, sizeof(data)));
	return 0;
}

static int test_tm_create_frame_invalid_params(void) {
	sdlp_tm_frame_t frame;
	uint8_t payload[1] = {0xAA};

	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tm_create_frame(NULL, 1, 1, payload, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tm_create_frame(&frame, 1, 1, NULL, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tm_create_frame(&frame, 1, 1, payload, TM_MAX_DATA_SIZE + 1));

	return 0;
}

static int test_tm_encode_decode_roundtrip(void) {
	sdlp_tm_frame_t frame;
	sdlp_tm_frame_t decoded;
	const uint8_t payload[] = {0x10, 0x20, 0x30, 0x40, 0x50};
	uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_create_frame(&frame, 0x7FF, 0x1F, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	ASSERT_EQ_INT(TM_PRIMARY_HEADER_SIZE + (int)sizeof(payload) + TM_FRAME_ERROR_CONTROL_SIZE,
								(int)encoded_size);

	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tm_decode_frame(encoded, encoded_size, &decoded));
	ASSERT_EQ_INT(SDLP_VERSION, decoded.header.transfer_frame_version);
	ASSERT_EQ_INT((int)(0x7FF & 0x3FF), decoded.header.spacecraft_id);
	ASSERT_EQ_INT((int)(0x1F & 0x07), decoded.header.virtual_channel_id);
	ASSERT_EQ_INT(frame.header.master_channel_frame_count,
								decoded.header.master_channel_frame_count);
	ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
	ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

	return 0;
}

static int test_tm_encode_buffer_too_small(void) {
	sdlp_tm_frame_t frame;
	const uint8_t payload[] = {0x01, 0x02, 0x03};
	uint8_t encoded[TM_PRIMARY_HEADER_SIZE + 2 + TM_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_create_frame(&frame, 2, 1, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_ERROR_BUFFER_TOO_SMALL,
								sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	return 0;
}

static int test_tm_decode_crc_mismatch(void) {
	sdlp_tm_frame_t frame;
	sdlp_tm_frame_t decoded;
	const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
	uint8_t encoded[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_create_frame(&frame, 3, 2, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tm_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	encoded[TM_PRIMARY_HEADER_SIZE] ^= 0x01;

	ASSERT_EQ_INT(SDLP_ERROR_CRC_MISMATCH,
								sdlp_tm_decode_frame(encoded, encoded_size, &decoded));

	return 0;
}

static int test_tc_create_frame_invalid_params(void) {
	sdlp_tc_frame_t frame;
	uint8_t payload[1] = {0x55};

	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tc_create_frame(NULL, 1, 1, 1, payload, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tc_create_frame(&frame, 1, 1, 1, NULL, 1));
	ASSERT_EQ_INT(SDLP_ERROR_INVALID_PARAM,
								sdlp_tc_create_frame(&frame, 1, 1, 1, payload, TC_MAX_DATA_SIZE + 1));

	return 0;
}

static int test_tc_encode_decode_roundtrip(void) {
	sdlp_tc_frame_t frame;
	sdlp_tc_frame_t decoded;
	const uint8_t payload[] = {0x01, 0x23, 0x45, 0x67};
	uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tc_create_frame(&frame, 0x7FF, 0x7F, 0x9A, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	ASSERT_EQ_INT(TC_PRIMARY_HEADER_SIZE + (int)sizeof(payload) + TC_FRAME_ERROR_CONTROL_SIZE,
								(int)encoded_size);

	ASSERT_EQ_INT(SDLP_SUCCESS, sdlp_tc_decode_frame(encoded, encoded_size, &decoded));
	ASSERT_EQ_INT(SDLP_VERSION, decoded.header.transfer_frame_version);
	ASSERT_EQ_INT((int)(0x7FF & 0x3FF), decoded.header.spacecraft_id);
	ASSERT_EQ_INT((int)(0x7F & 0x3F), decoded.header.virtual_channel_id);
	ASSERT_EQ_INT(0x9A, decoded.header.frame_sequence_number);
	ASSERT_EQ_INT((int)sizeof(payload) - 1, decoded.header.frame_length);
	ASSERT_EQ_INT((int)sizeof(payload), decoded.data_length);
	ASSERT_EQ_MEM(payload, decoded.data, sizeof(payload));

	return 0;
}

static int test_tc_encode_buffer_too_small(void) {
	sdlp_tc_frame_t frame;
	const uint8_t payload[] = {0xAB, 0xCD, 0xEF};
	uint8_t encoded[TC_PRIMARY_HEADER_SIZE + 2 + TC_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tc_create_frame(&frame, 5, 3, 7, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_ERROR_BUFFER_TOO_SMALL,
								sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	return 0;
}

static int test_tc_decode_crc_mismatch(void) {
	sdlp_tc_frame_t frame;
	sdlp_tc_frame_t decoded;
	const uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
	uint8_t encoded[TC_PRIMARY_HEADER_SIZE + TC_MAX_DATA_SIZE + TC_FRAME_ERROR_CONTROL_SIZE];
	size_t encoded_size = 0;

	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tc_create_frame(&frame, 0x12, 0x05, 0x42, payload, (uint16_t)sizeof(payload)));
	ASSERT_EQ_INT(SDLP_SUCCESS,
								sdlp_tc_encode_frame(&frame, encoded, sizeof(encoded), &encoded_size));

	encoded[TC_PRIMARY_HEADER_SIZE] ^= 0x80;

	ASSERT_EQ_INT(SDLP_ERROR_CRC_MISMATCH,
								sdlp_tc_decode_frame(encoded, encoded_size, &decoded));

	return 0;
}

int main(void) {
	RUN_TEST(test_crc16_known_vector);
	RUN_TEST(test_tm_create_frame_invalid_params);
	RUN_TEST(test_tm_encode_decode_roundtrip);
	RUN_TEST(test_tm_encode_buffer_too_small);
	RUN_TEST(test_tm_decode_crc_mismatch);
	RUN_TEST(test_tc_create_frame_invalid_params);
	RUN_TEST(test_tc_encode_decode_roundtrip);
	RUN_TEST(test_tc_encode_buffer_too_small);
	RUN_TEST(test_tc_decode_crc_mismatch);

	if (cunit_overall_failures) {
		printf("\nTotal failures: %d\n", cunit_overall_failures);
		return 1;
	}

	printf("\nAll tests passed.\n");
	return 0;
}
