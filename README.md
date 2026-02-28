# EmbeddedSDLP

Minimal, embedded-optimized implementation of **CCSDS Space Data Link Protocol (SDLP)** for TM (Telemetry) and TC (Telecommand) frame handling, following international standards.

## Standards Compliance

- **CCSDS 132.0-B-3**: TM Space Data Link Protocol
- **CCSDS 232.0-B-4**: TC Space Data Link Protocol

## Features

### Core Protocol Implementation

- **Telemetry (TM) Frame Handling**: Create, encode, and decode TM frames with CRC validation
- **Telecommand (TC) Frame Handling**: Create, encode, and decode TC frames with CRC validation
- **CRC16 Error Detection**: Built-in frame error control field (FECF) for data integrity
- **Configurable**: Support for virtual channels, spacecraft IDs, and frame sequence numbers
- **TC Segment Header**: Optional MAP-based segmentation support (enabled with `TC_SEGMENT_HEADER_ENABLED`)

### Design Principles

- **Minimal footprint**: Small library size (stripped)
- **Zero allocation**: Stack-based, no dynamic memory
- **Embedded-optimized**: Pure C99, no external dependencies
- **Portable**: Standard C99, big-endian network byte order
- **Reliable**: CRC-16-CCITT frame error control

## Project Structure

```
EmbeddedSDLP/
├── include/
│   ├── sdlp_common.h    # Common definitions and CRC
│   ├── sdlp_tm.h        # TM frame definitions
│   └── sdlp_tc.h        # TC frame definitions
├── src/
│   ├── sdlp_common.c    # CRC16 implementation
│   ├── sdlp_tm.c        # TM frame implementation
│   └── sdlp_tc.c        # TC frame implementation
├── examples/
│   ├── tm_example.c     # TM frame example
│   └── tc_example.c     # TC frame example
├── docs/
│   ├── 132x0b3_TM_SDLP.pdf   # CCSDS 132.0-B-3 standard
│   └── 232x0b4e1c1_TC_SDLP.pdf # CCSDS 232.0-B-4 standard
├── Makefile
└── README.md
```

## Building

### Build Everything

```bash
make all
```

This will create:
- `build/libsdlp.a` - Static library
- `build/bin/tm_example` - TM frame example
- `build/bin/tc_example` - TC frame example

### Build Library Only

```bash
make lib
# Produces: build/libsdlp.a (static)
```

### Build Examples

```bash
make examples
```

### Run Tests

```bash
make test
```

### Coverage (HTML)

Requires `gcovr` installed in your system:

```bash
sudo apt install gcovr
```

Generate coverage report:

```bash
make coverage-html
```

Output report:

```text
build/coverage/index.html
```

### Clean

```bash
make clean      # Remove build artifacts
```

## Quick Start

### Create and Send a TM Frame

```c
#include "sdlp_tm.h"

uint8_t buffer[1500];
size_t encoded_size;

// Create a TM frame
const char *data = "Temperature: 25C, Voltage: 3.3V";
sdlp_tm_frame_t frame;
sdlp_tm_create_frame(&frame, 0x123, 2, (uint8_t *)data, strlen(data));

// Encode frame to buffer
if (sdlp_tm_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size) == SDLP_SUCCESS) {
    printf("TM frame encoded: %zu bytes\n", encoded_size);
}
```

### Parse an Incoming TM Frame

```c
sdlp_tm_frame_t decoded;
if (sdlp_tm_decode_frame(buffer, encoded_size, &decoded) == SDLP_SUCCESS) {
    printf("Spacecraft ID: 0x%03X, Data: %.*s\n",
           decoded.header.spacecraft_id,
           (int)decoded.data_length, decoded.data);
}
```

### Create and Send a TC Frame

```c
#include "sdlp_tc.h"

/* Telecommand identifiers */
#define TC_CMD_SET_MODE_SAFE  0x01U

uint8_t buffer[1500];
size_t encoded_size;

// Create a TC frame with a numeric command ID
uint8_t cmd_id = TC_CMD_SET_MODE_SAFE;
sdlp_tc_frame_t frame;
sdlp_tc_create_frame(&frame, 0x123, 1, 42, &cmd_id, sizeof(cmd_id));

// Encode frame to buffer
if (sdlp_tc_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size) == SDLP_SUCCESS) {
    printf("TC frame encoded: %zu bytes\n", encoded_size);
}
```

### Parse an Incoming TC Frame

```c
sdlp_tc_frame_t decoded;
if (sdlp_tc_decode_frame(buffer, encoded_size, &decoded) == SDLP_SUCCESS) {
    printf("Spacecraft ID: 0x%03X, Command ID: 0x%02X\n",
           decoded.header.spacecraft_id,
           decoded.data[0]);
}
```

## API Reference

### Common

```c
// Calculate CRC-16-CCITT checksum
uint16_t sdlp_crc16(const uint8_t *data, size_t length);
```

### TM Functions

```c
// Create a TM frame with payload data
int sdlp_tm_create_frame(sdlp_tm_frame_t *frame, uint16_t spacecraft_id,
                          uint8_t virtual_channel_id, const uint8_t *data,
                          uint16_t data_length);

// Encode a TM frame into a byte buffer
int sdlp_tm_encode_frame(const sdlp_tm_frame_t *frame, uint8_t *buffer,
                          size_t buffer_size, size_t *encoded_size);

// Decode a TM frame from a byte buffer (validates CRC)
int sdlp_tm_decode_frame(const uint8_t *buffer, size_t buffer_size,
                          sdlp_tm_frame_t *frame);
```

### TC Functions

```c
// Create a TC frame with command data
int sdlp_tc_create_frame(sdlp_tc_frame_t *frame, uint16_t spacecraft_id,
                          uint8_t virtual_channel_id, uint8_t frame_seq_num,
                          const uint8_t *data, uint16_t data_length);

// Encode a TC frame into a byte buffer
int sdlp_tc_encode_frame(const sdlp_tc_frame_t *frame, uint8_t *buffer,
                          size_t buffer_size, size_t *encoded_size);

// Decode a TC frame from a byte buffer (validates CRC)
int sdlp_tc_decode_frame(const uint8_t *buffer, size_t buffer_size,
                          sdlp_tc_frame_t *frame);

// Set TC segment header fields (requires TC_SEGMENT_HEADER_ENABLED)
int sdlp_tc_set_segment_header(sdlp_tc_frame_t *frame,
                                sdlp_tc_seq_flag_t sequence_flags, uint8_t map_id);
```

All functions return `SDLP_SUCCESS` (0) on success or a negative error code on failure:
- `SDLP_ERROR_INVALID_PARAM` (-1): NULL pointer or invalid parameter
- `SDLP_ERROR_BUFFER_TOO_SMALL` (-2): Output buffer too small
- `SDLP_ERROR_INVALID_FRAME` (-3): Frame structure invalid
- `SDLP_ERROR_CRC_MISMATCH` (-4): CRC validation failed

## Memory Usage (Estimated)

- **Library (stripped)**: < 5 KB
- **Per TM frame buffer**: `TM_PRIMARY_HEADER_SIZE` (6) + data + 2 bytes FECF
- **Per TC frame buffer**: `TC_PRIMARY_HEADER_SIZE` (5) + data + 2 bytes FECF
- **Maximum data per frame**: 1024 bytes (`TM_MAX_DATA_SIZE` / `TC_MAX_DATA_SIZE`)

## Limitations and Extensions

Current implementation focuses on core protocol features:

- No automatic retransmission handling
- No flow control or bandwidth management
- No segmentation beyond optional TC segment header
- Single static frame counter (not thread-safe)

These can be extended as needed for specific mission requirements.

## References

- CCSDS 132.0-B-3: TM Space Data Link Protocol ([docs/132x0b3_TM_SDLP.pdf](docs/132x0b3_TM_SDLP.pdf))
- CCSDS 232.0-B-4: TC Space Data Link Protocol ([docs/232x0b4e1c1_TC_SDLP.pdf](docs/232x0b4e1c1_TC_SDLP.pdf))

## License

Apache License 2.0 - See [LICENSE](LICENSE) file for details.
