#include <stdio.h>
#include <string.h>
#include "sdlp_tc.h"

int main(void) {
    sdlp_tc_frame_t frame;
    uint8_t buffer[1500];
    size_t encoded_size;
    int result;
    
    printf("=== TC Frame Example ===\n\n");
    
    const char *command_data = "SET_MODE SAFE";
    uint16_t spacecraft_id = 0x123;
    uint8_t virtual_channel = 1;
    uint8_t sequence_number = 42;
    
    printf("Creating TC frame...\n");
    printf("Spacecraft ID: 0x%03X\n", spacecraft_id);
    printf("Virtual Channel: %d\n", virtual_channel);
    printf("Sequence Number: %d\n", sequence_number);
    printf("Command: %s\n", command_data);
    
    result = sdlp_tc_create_frame(&frame, spacecraft_id, virtual_channel,
                                   sequence_number, (const uint8_t *)command_data,
                                   strlen(command_data));
    
    if (result != SDLP_SUCCESS) {
        printf("Error creating frame: %d\n", result);
        return 1;
    }

#ifdef TC_SEGMENT_HEADER_ENABLED
    /* Attach a segment header: no segmentation, MAP ID = 0 (CCSDS 232.0-B-4 4.1.3.2.2) */
    result = sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_NO_SEG, 0);
    if (result != SDLP_SUCCESS) {
        printf("Error setting segment header: %d\n", result);
        return 1;
    }
    printf("Segment Header: sequence_flags=0x%X map_id=%d\n",
           frame.segment_header.sequence_flags, frame.segment_header.map_id);
#endif
    
    printf("\nEncoding frame...\n");
    result = sdlp_tc_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);
    
    if (result != SDLP_SUCCESS) {
        printf("Error encoding frame: %d\n", result);
        return 1;
    }
    
    printf("Encoded frame size: %zu bytes\n", encoded_size);
    printf("Frame bytes: ");
    for (size_t i = 0; i < encoded_size && i < 20; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("...\n");
    
    printf("\nDecoding frame...\n");
    sdlp_tc_frame_t decoded_frame;
    memset(&decoded_frame, 0, sizeof(decoded_frame));
#ifdef TC_SEGMENT_HEADER_ENABLED
    decoded_frame.use_segment_header = 1;
#endif
    result = sdlp_tc_decode_frame(buffer, encoded_size, &decoded_frame);
    
    if (result != SDLP_SUCCESS) {
        printf("Error decoding frame: %d\n", result);
        return 1;
    }
    
    printf("Decoded successfully!\n");
    printf("Spacecraft ID: 0x%03X\n", decoded_frame.header.spacecraft_id);
    printf("Virtual Channel: %d\n", decoded_frame.header.virtual_channel_id);
    printf("Frame Length: %d\n", decoded_frame.header.frame_length);
#ifdef TC_SEGMENT_HEADER_ENABLED
    printf("Segment Header: sequence_flags=0x%X map_id=%d\n",
           decoded_frame.segment_header.sequence_flags, decoded_frame.segment_header.map_id);
#endif
    printf("Command: %.*s\n", (int)decoded_frame.data_length, decoded_frame.data);
    printf("CRC: 0x%04X\n", decoded_frame.fecf);
    
    printf("\n=== TC Frame Example Complete ===\n");
    
    return 0;
}
