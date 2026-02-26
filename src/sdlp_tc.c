#include "sdlp_tc.h"
#include <string.h>

int sdlp_tc_create_frame(sdlp_tc_frame_t *frame, uint16_t spacecraft_id, 
                          uint8_t virtual_channel_id, uint8_t frame_seq_num,
                          const uint8_t *data, uint16_t data_length) {
    if (!frame || !data || data_length > TC_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_PARAM;
    }

    memset(frame, 0, sizeof(sdlp_tc_frame_t));
    
    frame->header.transfer_frame_version = SDLP_VERSION;
    frame->header.bypass_flag = 0;
    frame->header.control_command_flag = 0;
    frame->header.reserved = 0;
    frame->header.spacecraft_id = (uint16_t)(spacecraft_id & 0x3FFU);
    frame->header.virtual_channel_id = (uint8_t)(virtual_channel_id & 0x3FU);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    frame->header.frame_length = (uint16_t)(data_length - 1U);
#pragma GCC diagnostic pop
    frame->header.frame_sequence_number = frame_seq_num;
    
    memcpy(frame->data, data, data_length);
    frame->data_length = data_length;
    
    return SDLP_SUCCESS;
}

int sdlp_tc_encode_frame(const sdlp_tc_frame_t *frame, uint8_t *buffer, 
                          size_t buffer_size, size_t *encoded_size) {
    if (!frame || !buffer || !encoded_size) {
        return SDLP_ERROR_INVALID_PARAM;
    }
    
    size_t required_size = TC_PRIMARY_HEADER_SIZE + frame->data_length + 
                           TC_FRAME_ERROR_CONTROL_SIZE;

#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag) {
        required_size += TC_SEGMENT_HEADER_SIZE;
    }
#endif
    
    if (buffer_size < required_size) {
        return SDLP_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    
    buffer[offset++] = (uint8_t)((frame->header.transfer_frame_version << 6) | 
                       ((frame->header.bypass_flag & 0x01U) << 5) |
                       ((frame->header.control_command_flag & 0x01U) << 4) |
                       ((frame->header.reserved & 0x03U) << 2) |
                       ((frame->header.spacecraft_id >> 8) & 0x03U));
    buffer[offset++] = (uint8_t)(frame->header.spacecraft_id & 0xFFU);
    buffer[offset++] = (uint8_t)(((frame->header.virtual_channel_id & 0x3FU) << 2) | 0x00U);
    buffer[offset++] = (uint8_t)((frame->header.frame_length >> 8) & 0xFFU);
    buffer[offset++] = (uint8_t)(frame->header.frame_length & 0xFFU);

#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag) {
        buffer[offset++] = (uint8_t)(((frame->segment_header.sequence_flags & 0x03U) << 6) |
                           (frame->segment_header.map_id & 0x3FU));
    }
#endif
    
    memcpy(&buffer[offset], frame->data, frame->data_length);
    offset += frame->data_length;
    
    uint16_t crc = sdlp_crc16(buffer, offset);
    buffer[offset++] = (uint8_t)((crc >> 8) & 0xFFU);
    buffer[offset++] = (uint8_t)(crc & 0xFFU);
    
    *encoded_size = offset;
    
    return SDLP_SUCCESS;
}

int sdlp_tc_decode_frame(const uint8_t *buffer, size_t buffer_size, 
                          sdlp_tc_frame_t *frame) {
    if (!buffer || !frame || buffer_size < TC_PRIMARY_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE) {
        return SDLP_ERROR_INVALID_PARAM;
    }
    
    memset(frame, 0, sizeof(sdlp_tc_frame_t));

    size_t offset = 0;
    
    /* Suppress -Wconversion: assigning masked values to bitfields is intentional. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    frame->header.transfer_frame_version = (uint8_t)((buffer[offset] >> 6) & 0x03U);
    frame->header.bypass_flag = (uint8_t)((buffer[offset] >> 5) & 0x01U);
    frame->header.control_command_flag = (uint8_t)((buffer[offset] >> 4) & 0x01U);
    frame->header.reserved = (uint8_t)((buffer[offset] >> 2) & 0x03U);
    frame->header.spacecraft_id = (uint16_t)(((uint16_t)(buffer[offset] & 0x03U) << 8) | buffer[offset + 1]);
    offset += 2;
    
    frame->header.virtual_channel_id = (uint8_t)((buffer[offset] >> 2) & 0x3FU);
    offset++;
    
    frame->header.frame_length = (uint16_t)(((uint16_t)buffer[offset] << 8) | buffer[offset + 1]);
#pragma GCC diagnostic pop
    offset += 2;

#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag) {
        if (buffer_size < TC_PRIMARY_HEADER_SIZE + TC_SEGMENT_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE) {
            return SDLP_ERROR_INVALID_FRAME;
        }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        frame->segment_header.sequence_flags = (uint8_t)((buffer[offset] >> 6) & 0x03U);
        frame->segment_header.map_id = (uint8_t)(buffer[offset] & 0x3FU);
#pragma GCC diagnostic pop
        offset++;
        frame->data_length = (uint16_t)(buffer_size - TC_PRIMARY_HEADER_SIZE - TC_SEGMENT_HEADER_SIZE -
                             TC_FRAME_ERROR_CONTROL_SIZE);
    } else {
        frame->data_length = (uint16_t)(buffer_size - TC_PRIMARY_HEADER_SIZE - TC_FRAME_ERROR_CONTROL_SIZE);
    }
#else
    frame->data_length = (uint16_t)(buffer_size - TC_PRIMARY_HEADER_SIZE - TC_FRAME_ERROR_CONTROL_SIZE);
#endif
    
    if (frame->data_length > TC_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_FRAME;
    }
    
    memcpy(frame->data, &buffer[offset], frame->data_length);
    offset += frame->data_length;
    
    frame->fecf = (uint16_t)(((uint16_t)buffer[offset] << 8) | buffer[offset + 1]);
    
    uint16_t calculated_crc = sdlp_crc16(buffer, buffer_size - TC_FRAME_ERROR_CONTROL_SIZE);
    
    if (calculated_crc != frame->fecf) {
        return SDLP_ERROR_CRC_MISMATCH;
    }
    
    return SDLP_SUCCESS;
}

#ifdef TC_SEGMENT_HEADER_ENABLED
int sdlp_tc_set_segment_header(sdlp_tc_frame_t *frame, sdlp_tc_seq_flag_t sequence_flags, uint8_t map_id) {
    if (!frame) {
        return SDLP_ERROR_INVALID_PARAM;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    frame->segment_header.sequence_flags = (uint8_t)(sequence_flags & 0x03U);
    frame->segment_header.map_id = (uint8_t)(map_id & 0x3FU);
#pragma GCC diagnostic pop
    return SDLP_SUCCESS;
}
#endif
