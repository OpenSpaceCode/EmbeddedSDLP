#ifndef SDLP_TM_H
#define SDLP_TM_H

#include "sdlp_common.h"

#define TM_PRIMARY_HEADER_SIZE 6
#define TM_FRAME_ERROR_CONTROL_SIZE 2
#define TM_MAX_DATA_SIZE 1024

typedef struct {
    uint16_t transfer_frame_version : 2;
    uint16_t spacecraft_id : 10;
    uint16_t virtual_channel_id : 3;
    uint16_t ocf_flag : 1;
    uint8_t master_channel_frame_count;
    uint8_t virtual_channel_frame_count;
    uint16_t transfer_frame_data_field_status;
} sdlp_tm_header_t;

typedef struct {
    sdlp_tm_header_t header;
    uint8_t data[TM_MAX_DATA_SIZE];
    uint16_t data_length;
    uint16_t fecf;
} sdlp_tm_frame_t;

int sdlp_tm_create_frame(sdlp_tm_frame_t *frame, uint16_t spacecraft_id, 
                          uint8_t virtual_channel_id, const uint8_t *data, 
                          uint16_t data_length);

int sdlp_tm_encode_frame(const sdlp_tm_frame_t *frame, uint8_t *buffer, 
                          size_t buffer_size, size_t *encoded_size);

int sdlp_tm_decode_frame(const uint8_t *buffer, size_t buffer_size, 
                          sdlp_tm_frame_t *frame);

#endif
