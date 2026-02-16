# Embedded-SDLP

A minimal Space Data Link Protocol (SDLP) implementation in C designed for embedded and resource-constrained space applications.

## Overview

EmbeddedSDLP provides a lightweight implementation of TM (Telemetry) and TC (Telecommand) frame handling based on CCSDS standards. It's designed for use in spacecraft, CubeSats, and other space systems where code size and reliability are critical.

## Features

- **Telemetry (TM) Frame Handling**: Create, encode, and decode TM frames with CRC validation
- **Telecommand (TC) Frame Handling**: Create, encode, and decode TC frames with CRC validation
- **Minimal Dependencies**: Pure C99 with no external dependencies
- **Embedded-Friendly**: Small memory footprint and predictable behavior
- **CRC16 Error Detection**: Built-in frame error control field (FECF) for data integrity
- **Configurable**: Support for virtual channels, spacecraft IDs, and frame sequence numbers

## Building

Build the library and examples using Make:

```bash
make all        # Build library and examples
make lib        # Build library only
make examples   # Build examples only
make test       # Run example programs
make clean      # Clean build artifacts
```

This will create:
- `libsdlp.a` - Static library
- `bin/tm_example` - TM frame example
- `bin/tc_example` - TC frame example

## Usage

### Telemetry (TM) Example

```c
#include "sdlp_tm.h"

sdlp_tm_frame_t frame;
uint8_t buffer[1500];
size_t encoded_size;

// Create a TM frame
const char *data = "Temperature: 25C";
sdlp_tm_create_frame(&frame, 0x123, 2, (uint8_t *)data, strlen(data));

// Encode frame to buffer
sdlp_tm_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);

// Decode frame from buffer
sdlp_tm_frame_t decoded;
sdlp_tm_decode_frame(buffer, encoded_size, &decoded);
```

### Telecommand (TC) Example

```c
#include "sdlp_tc.h"

sdlp_tc_frame_t frame;
uint8_t buffer[1500];
size_t encoded_size;

// Create a TC frame
const char *cmd = "ENABLE_SENSOR";
sdlp_tc_create_frame(&frame, 0x456, 1, 0, (uint8_t *)cmd, strlen(cmd));

// Encode frame to buffer
sdlp_tc_encode_frame(&frame, buffer, sizeof(buffer), &encoded_size);

// Decode frame from buffer
sdlp_tc_frame_t decoded;
sdlp_tc_decode_frame(buffer, encoded_size, &decoded);
```

## API Overview

### Common Functions
- `sdlp_crc16()` - Calculate CRC16 checksum

### TM Functions
- `sdlp_tm_create_frame()` - Create a TM frame with data
- `sdlp_tm_encode_frame()` - Encode frame to byte buffer
- `sdlp_tm_decode_frame()` - Decode frame from byte buffer

### TC Functions
- `sdlp_tc_create_frame()` - Create a TC frame with data
- `sdlp_tc_encode_frame()` - Encode frame to byte buffer
- `sdlp_tc_decode_frame()` - Decode frame from byte buffer

All functions return `SDLP_SUCCESS` (0) on success or a negative error code on failure.

## Project Structure

```
.
├── include/          # Header files
│   ├── sdlp_common.h # Common definitions and CRC
│   ├── sdlp_tm.h     # TM frame definitions
│   └── sdlp_tc.h     # TC frame definitions
├── src/              # Implementation files
│   ├── sdlp_common.c
│   ├── sdlp_tm.c
│   └── sdlp_tc.c
├── examples/         # Example programs
│   ├── tm_example.c
│   └── tc_example.c
└── Makefile          # Build configuration
```

## Requirements

- C compiler with C99 support (gcc, clang, etc.)
- Make

## License

Apache License 2.0 - See [LICENSE](LICENSE) file for details.
