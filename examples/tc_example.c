/**
 * @file tc_example.c
 * @brief Worked example: build, encode, and decode TC Transfer Frames.
 *
 * Demonstrates a Type-AD data frame with application-side FECF computation and a
 * Type-BC Unlock control-command frame.
 */

#include "example_crc.h"
#include "sdlp_tc.h"

#include <stdio.h>

/* Example telecommand identifiers */
#define TC_CMD_SET_MODE_SAFE 0x01U

/** @brief Number of leading frame octets shown in the hex dump. */
#define EXAMPLE_HEX_DUMP_OCTETS 20u

int main(void)
{
    sdlp_tc_frame_t frame;
    uint8_t buffer[TC_MAX_FRAME_SIZE];
    size_t encoded_size;
    sdlp_status_t result;

    printf("=== TC Frame Example ===\n\n");

    uint8_t cmd_id = TC_CMD_SET_MODE_SAFE;
    uint16_t spacecraft_id = 0x123u;
    uint8_t virtual_channel = 1;
    uint8_t sequence_number = 42;

    printf("Creating TC frame...\n");
    printf("Spacecraft ID: 0x%03X\n", spacecraft_id);
    printf("Virtual Channel: %d\n", virtual_channel);
    printf("Sequence Number: %d\n", sequence_number);
    printf("Command ID: 0x%02X\n", cmd_id);

    result = sdlp_tc_create_frame(&frame,
                                  spacecraft_id,
                                  virtual_channel,
                                  sequence_number,
                                  &cmd_id,
                                  sizeof(cmd_id));

    if (result != SDLP_SUCCESS)
    {
        printf("Error creating frame: %d\n", result);
        return 1;
    }

#ifdef TC_SEGMENT_HEADER_ENABLED
    /* Attach a segment header: no segmentation, MAP ID = 0 (CCSDS 232.0-B-4 4.1.3.2.2) */
    result = sdlp_tc_set_segment_header(&frame, TC_SEQ_FLAG_NO_SEG, 0);
    if (result != SDLP_SUCCESS)
    {
        printf("Error setting segment header: %d\n", result);
        return 1;
    }
    printf("Segment Header: sequence_flags=0x%X map_id=%d\n",
           frame.segment_header.sequence_flags,
           frame.segment_header.map_id);
#endif

    printf("\nEncoding frame...\n");
    result = sdlp_tc_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);

    if (result != SDLP_SUCCESS)
    {
        printf("Error encoding frame: %d\n", result);
        return 1;
    }

    /* The library leaves the Frame Error Control Field to the application.
     * Compute a CRC-16-CCITT over the encoded frame (excluding the trailing
     * 2-byte FECF) and write it into the FECF, mirroring CCSDS error control. */
    size_t fecf_offset = encoded_size - TC_FRAME_ERROR_CONTROL_SIZE;
    uint16_t fecf = example_crc16(buffer, fecf_offset);
    buffer[fecf_offset] = (uint8_t)((fecf >> 8) & 0xFFu);
    buffer[fecf_offset + 1] = (uint8_t)(fecf & 0xFFu);

    printf("Encoded frame size: %zu bytes\n", encoded_size);
    printf("Frame bytes: ");
    for (size_t i = 0; (i < encoded_size) && (i < EXAMPLE_HEX_DUMP_OCTETS); i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("...\n");

    printf("\nDecoding frame...\n");
    sdlp_tc_frame_t decoded_frame;
    result = sdlp_tc_decode_frame(buffer, encoded_size, &decoded_frame);

    if (result != SDLP_SUCCESS)
    {
        printf("Error decoding frame: %d\n", result);
        return 1;
    }

    printf("Decoded successfully!\n");
    printf("Spacecraft ID: 0x%03X\n", decoded_frame.header.spacecraft_id);
    printf("Virtual Channel: %d\n", decoded_frame.header.virtual_channel_id);
    printf("Frame Length: %d\n", decoded_frame.header.frame_length);
#ifdef TC_SEGMENT_HEADER_ENABLED
    printf("Segment Header: sequence_flags=0x%X map_id=%d\n",
           decoded_frame.segment_header.sequence_flags,
           decoded_frame.segment_header.map_id);
#endif
    printf("Command ID: 0x%02X\n", decoded_frame.data[0]);

    /* Verify the FECF the same way it was produced (application-side). */
    uint16_t expected_fecf = example_crc16(buffer, encoded_size - TC_FRAME_ERROR_CONTROL_SIZE);
    printf("FECF: 0x%04X (%s)\n",
           decoded_frame.fecf,
           decoded_frame.fecf == expected_fecf ? "valid" : "invalid");

    /* Type-BC frames carry Control Commands for the FARM instead of data
     * (CCSDS 232.0-B-4, 4.1.3.3). Send an Unlock command. */
    printf("\nCreating Unlock control command frame (Type-BC)...\n");
    result = sdlp_tc_create_unlock_frame(&frame, spacecraft_id, virtual_channel);

    if (result != SDLP_SUCCESS)
    {
        printf("Error creating unlock frame: %d\n", result);
        return 1;
    }

    result = sdlp_tc_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);

    if (result != SDLP_SUCCESS)
    {
        printf("Error encoding unlock frame: %d\n", result);
        return 1;
    }

    fecf_offset = encoded_size - TC_FRAME_ERROR_CONTROL_SIZE;
    uint16_t unlock_fecf = example_crc16(buffer, fecf_offset);
    buffer[fecf_offset] = (uint8_t)((unlock_fecf >> 8) & 0xFFu);
    buffer[fecf_offset + 1] = (uint8_t)(unlock_fecf & 0xFFu);

    printf("Encoded frame size: %zu bytes\n", encoded_size);

    result = sdlp_tc_decode_frame(buffer, encoded_size, &decoded_frame);

    if (result != SDLP_SUCCESS)
    {
        printf("Error decoding unlock frame: %d\n", result);
        return 1;
    }

    printf("Bypass Flag: %d, Control Command Flag: %d (Type-BC)\n",
           decoded_frame.header.bypass_flag,
           decoded_frame.header.control_command_flag);
    printf("Control Command: 0x%02X (%s)\n",
           decoded_frame.data[0],
           decoded_frame.data[0] == TC_CONTROL_CMD_UNLOCK ? "Unlock" : "unknown");

    printf("\n=== TC Frame Example Complete ===\n");

    return 0;
}
