#include <stdio.h>
#include <string.h>
#include "sdlp_tm.h"

int main(void) {
    sdlp_tm_frame_t frame;
    uint8_t buffer[1500];
    size_t encoded_size;
    int result;
    
    printf("=== TM Frame Example ===\n\n");
    
    const char *telemetry_data = "Temperature: 25C, Voltage: 3.3V";
    uint16_t spacecraft_id = 0x123;
    uint8_t virtual_channel = 2;
    
    printf("Creating TM frame...\n");
    printf("Spacecraft ID: 0x%03X\n", spacecraft_id);
    printf("Virtual Channel: %d\n", virtual_channel);
    printf("Data: %s\n", telemetry_data);
    
    result = sdlp_tm_create_frame(&frame, spacecraft_id, virtual_channel,
                                   (const uint8_t *)telemetry_data,
                                   strlen(telemetry_data));
    
    if (result != SDLP_SUCCESS) {
        printf("Error creating frame: %d\n", result);
        return 1;
    }
    
    printf("\nEncoding frame...\n");
    result = sdlp_tm_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);
    
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
    sdlp_tm_frame_t decoded_frame;
    result = sdlp_tm_decode_frame(buffer, encoded_size, &decoded_frame);
    
    if (result != SDLP_SUCCESS) {
        printf("Error decoding frame: %d\n", result);
        return 1;
    }
    
    printf("Decoded successfully!\n");
    printf("Spacecraft ID: 0x%03X\n", decoded_frame.header.spacecraft_id);
    printf("Virtual Channel: %d\n", decoded_frame.header.virtual_channel_id);
    printf("Frame Count: %d\n", decoded_frame.header.master_channel_frame_count);
    printf("Data: %.*s\n", (int)decoded_frame.data_length, decoded_frame.data);
    printf("CRC: 0x%04X\n", decoded_frame.fecf);
    
    printf("\n=== TM Frame Example Complete ===\n");
    
    return 0;
}
