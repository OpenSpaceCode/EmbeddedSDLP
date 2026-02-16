#ifndef SDLP_TC_H
#define SDLP_TC_H

#include "sdlp_common.h"

#define TC_PRIMARY_HEADER_SIZE 5
#define TC_FRAME_ERROR_CONTROL_SIZE 2
#define TC_MAX_DATA_SIZE 1024

typedef struct {
    uint16_t transfer_frame_version : 2;
    uint16_t bypass_flag : 1;
    uint16_t control_command_flag : 1;
    uint16_t reserved : 2;
    uint16_t spacecraft_id : 10;
    uint8_t virtual_channel_id : 6;
    uint16_t frame_length : 10;
    uint8_t frame_sequence_number;
} sdlp_tc_header_t;

typedef struct {
    sdlp_tc_header_t header;
    uint8_t data[TC_MAX_DATA_SIZE];
    uint16_t data_length;
    uint16_t fecf;
} sdlp_tc_frame_t;

int sdlp_tc_create_frame(sdlp_tc_frame_t *frame, uint16_t spacecraft_id, 
                          uint8_t virtual_channel_id, uint8_t frame_seq_num,
                          const uint8_t *data, uint16_t data_length);

int sdlp_tc_encode_frame(const sdlp_tc_frame_t *frame, uint8_t *buffer, 
                          size_t buffer_size, size_t *encoded_size);

int sdlp_tc_decode_frame(const uint8_t *buffer, size_t buffer_size, 
                          sdlp_tc_frame_t *frame);

#endif
