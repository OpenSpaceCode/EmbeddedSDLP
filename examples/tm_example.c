/**
 * @file tm_example.c
 * @brief Worked example: build, encode, and decode a TM Transfer Frame.
 *
 * Demonstrates a plain data frame, application-side FECF computation, and an
 * optional Operational Control Field.
 */

#include "example_crc.h"
#include "sdlp_tm.h"

#include <stdio.h>
#include <string.h>

/** @brief Number of leading frame octets shown in the hex dump. */
#define EXAMPLE_HEX_DUMP_OCTETS 20u

int main(void)
{
    sdlp_tm_frame_t frame;
    uint8_t buffer[TM_PRIMARY_HEADER_SIZE + TM_MAX_DATA_SIZE + TM_OCF_SIZE +
                   TM_FRAME_ERROR_CONTROL_SIZE];
    size_t encoded_size;
    sdlp_status_t result;

    printf("=== TM Frame Example ===\n\n");

    const char *telemetry_data = "Temperature: 25C, Voltage: 3.3V";
    uint16_t spacecraft_id = 0x123u;
    uint8_t virtual_channel = 2;

    printf("Creating TM frame...\n");
    printf("Spacecraft ID: 0x%03X\n", spacecraft_id);
    printf("Virtual Channel: %d\n", virtual_channel);
    printf("Data: %s\n", telemetry_data);

    result = sdlp_tm_create_frame(&frame,
                                  spacecraft_id,
                                  virtual_channel,
                                  (const uint8_t *)telemetry_data,
                                  (uint16_t)strlen(telemetry_data));

    if (result != SDLP_SUCCESS)
    {
        printf("Error creating frame: %d\n", result);
        return 1;
    }

    printf("\nEncoding frame...\n");
    result = sdlp_tm_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);

    if (result != SDLP_SUCCESS)
    {
        printf("Error encoding frame: %d\n", result);
        return 1;
    }

    /* The library leaves the Frame Error Control Field to the application.
     * Compute a CRC-16-CCITT over the encoded frame (excluding the trailing
     * 2-byte FECF) and write it into the FECF, mirroring CCSDS error control. */
    size_t fecf_offset = encoded_size - TM_FRAME_ERROR_CONTROL_SIZE;
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
    sdlp_tm_frame_t decoded_frame;
    result = sdlp_tm_decode_frame(buffer, encoded_size, &decoded_frame);

    if (result != SDLP_SUCCESS)
    {
        printf("Error decoding frame: %d\n", result);
        return 1;
    }

    printf("Decoded successfully!\n");
    printf("Spacecraft ID: 0x%03X\n", decoded_frame.header.spacecraft_id);
    printf("Virtual Channel: %d\n", decoded_frame.header.virtual_channel_id);
    printf("Frame Count: %d\n", decoded_frame.header.master_channel_frame_count);
    printf("Data: %.*s\n", (int)decoded_frame.data_length, decoded_frame.data);

    /* Verify the FECF the same way it was produced (application-side). */
    uint16_t expected_fecf = example_crc16(buffer, encoded_size - TM_FRAME_ERROR_CONTROL_SIZE);
    printf("FECF: 0x%04X (%s)\n",
           decoded_frame.fecf,
           decoded_frame.fecf == expected_fecf ? "valid" : "invalid");

    /* Optionally attach a 4-octet Operational Control Field (CCSDS 132.0-B-3, 4.1.5).
     * Its content is mission-specific (e.g. a CLCW); here it is opaque. Setting it
     * raises the OCF Flag; leaving it unset emits a frame with no OCF. */
    printf("\nEncoding the same frame with an Operational Control Field...\n");
    const uint8_t clcw[TM_OCF_SIZE] = {0};
    result = sdlp_tm_set_ocf(&frame, clcw);
    if (result != SDLP_SUCCESS)
    {
        printf("Error setting OCF: %d\n", result);
        return 1;
    }

    result = sdlp_tm_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);
    if (result != SDLP_SUCCESS)
    {
        printf("Error encoding frame: %d\n", result);
        return 1;
    }

    result = sdlp_tm_decode_frame(buffer, encoded_size, &decoded_frame);
    if (result != SDLP_SUCCESS)
    {
        printf("Error decoding frame: %d\n", result);
        return 1;
    }

    printf("Encoded frame size: %zu bytes\n", encoded_size);
    printf("OCF Flag: %d, OCF:", decoded_frame.header.ocf_flag);
    for (size_t i = 0; i < TM_OCF_SIZE; i++)
    {
        printf(" %02X", decoded_frame.ocf[i]);
    }
    printf("\n");

    printf("\n=== TM Frame Example Complete ===\n");

    return 0;
}
