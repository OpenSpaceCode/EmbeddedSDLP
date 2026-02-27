#include "sdlp_tm.h"
#include <string.h>

/* Note: not thread-safe, caller must ensure sequential access */
static uint8_t tm_frame_counter = 0;

int sdlp_tm_create_frame(sdlp_tm_frame_t *frame, uint16_t spacecraft_id, 
                          uint8_t virtual_channel_id, const uint8_t *data, 
                          uint16_t data_length) {
    if (!frame || !data || data_length > TM_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_PARAM;
    }

    memset(frame, 0, sizeof(sdlp_tm_frame_t));
    
    frame->header.transfer_frame_version = SDLP_VERSION;
    frame->header.spacecraft_id = (uint16_t)(spacecraft_id & 0x3ffu);
    frame->header.virtual_channel_id = (uint8_t)(virtual_channel_id & 0x07u);
    frame->header.ocf_flag = 0;
    frame->header.master_channel_frame_count = tm_frame_counter++;
    frame->header.virtual_channel_frame_count = 0;
    frame->header.transfer_frame_data_field_status = 0;
    
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
    
    buffer[offset++] = (uint8_t)((frame->header.transfer_frame_version << 6) | 
                       ((frame->header.spacecraft_id >> 4) & 0x3fu));
    buffer[offset++] = (uint8_t)(((frame->header.spacecraft_id & 0x0fu) << 4) | 
                       ((frame->header.virtual_channel_id & 0x07u) << 1) | 
                       (frame->header.ocf_flag & 0x01u));
    buffer[offset++] = frame->header.master_channel_frame_count;
    buffer[offset++] = frame->header.virtual_channel_frame_count;
    
    uint16_t data_field_status = frame->header.transfer_frame_data_field_status;
    buffer[offset++] = (uint8_t)((data_field_status >> 8) & 0xffu);
    buffer[offset++] = (uint8_t)(data_field_status & 0xffu);
    
    memcpy(&buffer[offset], frame->data, frame->data_length);
    offset += frame->data_length;
    
    uint16_t crc = sdlp_crc16(buffer, offset);
    buffer[offset++] = (uint8_t)((crc >> 8) & 0xffu);
    buffer[offset++] = (uint8_t)(crc & 0xffu);
    
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
    
    frame->header.transfer_frame_version = (uint8_t)((buffer[offset] >> 6) & 0x03u);
    frame->header.spacecraft_id = (uint16_t)(((buffer[offset] & 0x3fu) << 4) | ((buffer[offset + 1] >> 4) & 0x0fu));
    offset++;
    
    frame->header.virtual_channel_id = (uint8_t)((buffer[offset] >> 1) & 0x07u);
    frame->header.ocf_flag = (uint8_t)(buffer[offset] & 0x01u);
    offset++;
    
    frame->header.master_channel_frame_count = buffer[offset++];
    frame->header.virtual_channel_frame_count = buffer[offset++];
    
    uint16_t data_field_status = (uint16_t)(((uint16_t)buffer[offset] << 8) | buffer[offset + 1]);
    frame->header.transfer_frame_data_field_status = data_field_status;
    offset += 2;
    
    frame->data_length = (uint16_t)(buffer_size - TM_PRIMARY_HEADER_SIZE - TM_FRAME_ERROR_CONTROL_SIZE);
    
    if (frame->data_length > TM_MAX_DATA_SIZE) {
        return SDLP_ERROR_INVALID_FRAME;
    }
    
    memcpy(frame->data, &buffer[offset], frame->data_length);
    offset += frame->data_length;
    
    frame->fecf = (uint16_t)(((uint16_t)buffer[offset] << 8) | buffer[offset + 1]);
    
    uint16_t calculated_crc = sdlp_crc16(buffer, buffer_size - TM_FRAME_ERROR_CONTROL_SIZE);
    
    if (calculated_crc != frame->fecf) {
        return SDLP_ERROR_CRC_MISMATCH;
    }
    
    return SDLP_SUCCESS;
}
