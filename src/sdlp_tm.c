#include "sdlp_tm.h"
#include <string.h>

static uint8_t tm_frame_counter = 0;

int sdlp_tm_create_frame(sdlp_tm_frame_t *frame, uint16_t spacecraft_id, 
                          uint8_t virtual_channel_id, const uint8_t *data, 
                          uint16_t data_length) {
    if (!frame || !data || data_length > TM_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_PARAM;
    }

    memset(frame, 0, sizeof(sdlp_tm_frame_t));
    
    frame->header.transfer_frame_version = SDLP_VERSION;
    frame->header.spacecraft_id = spacecraft_id & 0x3FF;
    frame->header.virtual_channel_id = virtual_channel_id & 0x07;
    frame->header.ocf_flag = 0;
    frame->header.master_channel_frame_count = tm_frame_counter++;
    frame->header.virtual_channel_frame_count = 0;
    frame->header.transfer_frame_data_field_status = 0;
    frame->header.sync_flag = 0;
    frame->header.packet_order_flag = 0;
    frame->header.segment_length_id = 3;
    frame->header.first_header_pointer = 0;
    
    memcpy(frame->data, data, data_length);
    frame->data_length = data_length;
    
    return SDLP_SUCCESS;
}

int sdlp_tm_encode_frame(const sdlp_tm_frame_t *frame, uint8_t *buffer, 
                          size_t buffer_size, size_t *encoded_size) {
    if (!frame || !buffer || !encoded_size) {
        return SDLP_ERROR_INVALID_PARAM;
    }
    
    size_t required_size = TM_PRIMARY_HEADER_SIZE + frame->data_length + 
                           TM_FRAME_ERROR_CONTROL_SIZE;
    
    if (buffer_size < required_size) {
        return SDLP_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    
    buffer[offset++] = (frame->header.transfer_frame_version << 6) | 
                       ((frame->header.spacecraft_id >> 4) & 0x3F);
    buffer[offset++] = ((frame->header.spacecraft_id & 0x0F) << 4) | 
                       ((frame->header.virtual_channel_id & 0x07) << 1) | 
                       (frame->header.ocf_flag & 0x01);
    buffer[offset++] = frame->header.master_channel_frame_count;
    buffer[offset++] = frame->header.virtual_channel_frame_count;
    
    uint16_t data_field_status = (frame->header.transfer_frame_data_field_status << 14) |
                                 (frame->header.sync_flag << 13) |
                                 (frame->header.packet_order_flag << 12) |
                                 (frame->header.segment_length_id << 10) |
                                 (frame->header.first_header_pointer & 0x7FF);
    buffer[offset++] = (data_field_status >> 8) & 0xFF;
    buffer[offset++] = data_field_status & 0xFF;
    
    memcpy(&buffer[offset], frame->data, frame->data_length);
    offset += frame->data_length;
    
    uint16_t crc = sdlp_crc16(buffer, offset);
    buffer[offset++] = (crc >> 8) & 0xFF;
    buffer[offset++] = crc & 0xFF;
    
    *encoded_size = offset;
    
    return SDLP_SUCCESS;
}

int sdlp_tm_decode_frame(const uint8_t *buffer, size_t buffer_size, 
                          sdlp_tm_frame_t *frame) {
    if (!buffer || !frame || buffer_size < TM_PRIMARY_HEADER_SIZE + TM_FRAME_ERROR_CONTROL_SIZE) {
        return SDLP_ERROR_INVALID_PARAM;
    }
    
    memset(frame, 0, sizeof(sdlp_tm_frame_t));
    
    size_t offset = 0;
    
    frame->header.transfer_frame_version = (buffer[offset] >> 6) & 0x03;
    frame->header.spacecraft_id = ((buffer[offset] & 0x3F) << 4) | ((buffer[offset + 1] >> 4) & 0x0F);
    offset++;
    
    frame->header.virtual_channel_id = (buffer[offset] >> 1) & 0x07;
    frame->header.ocf_flag = buffer[offset] & 0x01;
    offset++;
    
    frame->header.master_channel_frame_count = buffer[offset++];
    frame->header.virtual_channel_frame_count = buffer[offset++];
    
    uint16_t data_field_status = (buffer[offset] << 8) | buffer[offset + 1];
    frame->header.transfer_frame_data_field_status = (data_field_status >> 14) & 0x03;
    frame->header.sync_flag = (data_field_status >> 13) & 0x01;
    frame->header.packet_order_flag = (data_field_status >> 12) & 0x01;
    frame->header.segment_length_id = (data_field_status >> 10) & 0x03;
    frame->header.first_header_pointer = data_field_status & 0x7FF;
    offset += 2;
    
    frame->data_length = buffer_size - TM_PRIMARY_HEADER_SIZE - TM_FRAME_ERROR_CONTROL_SIZE;
    
    if (frame->data_length > TM_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_FRAME;
    }
    
    memcpy(frame->data, &buffer[offset], frame->data_length);
    offset += frame->data_length;
    
    frame->fecf = (buffer[offset] << 8) | buffer[offset + 1];
    
    uint16_t calculated_crc = sdlp_crc16(buffer, buffer_size - TM_FRAME_ERROR_CONTROL_SIZE);
    
    if (calculated_crc != frame->fecf) {
        return SDLP_ERROR_CRC_MISMATCH;
    }
    
    return SDLP_SUCCESS;
}
