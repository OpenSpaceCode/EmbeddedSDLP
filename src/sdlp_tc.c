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
    frame->header.spacecraft_id = spacecraft_id & 0x3FF;
    frame->header.virtual_channel_id = virtual_channel_id & 0x3F;
    frame->header.frame_length = data_length - 1;
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
    
    buffer[offset++] = (frame->header.transfer_frame_version << 6) | 
                       ((frame->header.bypass_flag & 0x01) << 5) |
                       ((frame->header.control_command_flag & 0x01) << 4) |
                       ((frame->header.reserved & 0x03) << 2) |
                       ((frame->header.spacecraft_id >> 8) & 0x03);
    buffer[offset++] = frame->header.spacecraft_id & 0xFF;
    buffer[offset++] = ((frame->header.virtual_channel_id & 0x3F) << 2) | 0x00;
    buffer[offset++] = (frame->header.frame_length >> 8) & 0xFF;
    buffer[offset++] = frame->header.frame_length & 0xFF;

#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag) {
        buffer[offset++] = ((frame->segment_header.sequence_flags & 0x03) << 6) |
                           (frame->segment_header.map_id & 0x3F);
    }
#endif
    
    memcpy(&buffer[offset], frame->data, frame->data_length);
    offset += frame->data_length;
    
    uint16_t crc = sdlp_crc16(buffer, offset);
    buffer[offset++] = (crc >> 8) & 0xFF;
    buffer[offset++] = crc & 0xFF;
    
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
    
    frame->header.transfer_frame_version = (buffer[offset] >> 6) & 0x03;
    frame->header.bypass_flag = (buffer[offset] >> 5) & 0x01;
    frame->header.control_command_flag = (buffer[offset] >> 4) & 0x01;
    frame->header.reserved = (buffer[offset] >> 2) & 0x03;
    frame->header.spacecraft_id = ((buffer[offset] & 0x03) << 8) | buffer[offset + 1];
    offset += 2;
    
    frame->header.virtual_channel_id = (buffer[offset] >> 2) & 0x3F;
    offset++;
    
    frame->header.frame_length = (buffer[offset] << 8) | buffer[offset + 1];
    offset += 2;

#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag) {
        if (buffer_size < TC_PRIMARY_HEADER_SIZE + TC_SEGMENT_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE) {
            return SDLP_ERROR_INVALID_FRAME;
        }
        frame->segment_header.sequence_flags = (buffer[offset] >> 6) & 0x03;
        frame->segment_header.map_id = buffer[offset] & 0x3F;
        offset++;
        frame->data_length = buffer_size - TC_PRIMARY_HEADER_SIZE - TC_SEGMENT_HEADER_SIZE -
                             TC_FRAME_ERROR_CONTROL_SIZE;
    } else {
        frame->data_length = buffer_size - TC_PRIMARY_HEADER_SIZE - TC_FRAME_ERROR_CONTROL_SIZE;
    }
#else
    frame->data_length = buffer_size - TC_PRIMARY_HEADER_SIZE - TC_FRAME_ERROR_CONTROL_SIZE;
#endif
    
    if (frame->data_length > TC_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_FRAME;
    }
    
    memcpy(frame->data, &buffer[offset], frame->data_length);
    offset += frame->data_length;
    
    frame->fecf = (buffer[offset] << 8) | buffer[offset + 1];
    
    uint16_t calculated_crc = sdlp_crc16(buffer, buffer_size - TC_FRAME_ERROR_CONTROL_SIZE);
    
    if (calculated_crc != frame->fecf) {
        return SDLP_ERROR_CRC_MISMATCH;
    }
    
    return SDLP_SUCCESS;
}

#ifdef TC_SEGMENT_HEADER_ENABLED
int sdlp_tc_set_segment_header(sdlp_tc_frame_t *frame, uint8_t sequence_flags, uint8_t map_id) {
    if (!frame) {
        return SDLP_ERROR_INVALID_PARAM;
    }
    frame->segment_header.sequence_flags = sequence_flags & 0x03;
    frame->segment_header.map_id = map_id & 0x3F;
    return SDLP_SUCCESS;
}
#endif
