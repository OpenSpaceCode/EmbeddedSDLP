#ifndef SDLP_COMMON_H
#define SDLP_COMMON_H

#include <stdint.h>
#include <stddef.h>

#define SDLP_VERSION 0

#define SDLP_SUCCESS 0
#define SDLP_ERROR_INVALID_PARAM -1
#define SDLP_ERROR_BUFFER_TOO_SMALL -2
#define SDLP_ERROR_INVALID_FRAME -3
#define SDLP_ERROR_CRC_MISMATCH -4

uint16_t sdlp_crc16(const uint8_t *data, size_t length);

#endif
