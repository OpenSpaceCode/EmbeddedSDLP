/**
 * @file sdlp_tc.h
 * @brief CCSDS TC Space Data Link Protocol frame handling (CCSDS 232.0-B-4).
 *
 * Builds, encodes, and decodes TC Transfer Frames: the 5-octet primary header,
 * an optional Segment Header, the Data Field, and the 2-octet Frame Error Control
 * Field. Supports the AD/BD/BC frame types and the Unlock / Set V(R) control
 * commands. The Data Field and FECF content are treated as opaque, application-
 * owned octet sequences.
 */

#ifndef SDLP_TC_H
#define SDLP_TC_H

#include "sdlp_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Transfer Frame Primary Header size in octets (§4.1.2). */
#define TC_PRIMARY_HEADER_SIZE 5

/** @brief Frame Error Control Field size in octets (§4.1.4). */
#define TC_FRAME_ERROR_CONTROL_SIZE 2

/** @brief Maximum Transfer Frame size in octets (§4.1.2.7). */
#define TC_MAX_FRAME_SIZE 1024

/**
 * @brief Maximum Transfer Frame Data Field length (CCSDS 232.0-B-4 §4.1.3.1.2).
 *
 * The frame minus the primary header and the Frame Error Control Field. When a
 * Segment Header is present it consumes one further octet of the Data Field
 * (enforced by sdlp_tc_create_frame()).
 */
#define TC_MAX_DATA_SIZE (TC_MAX_FRAME_SIZE - TC_PRIMARY_HEADER_SIZE - TC_FRAME_ERROR_CONTROL_SIZE)

/** @brief Frame Length field = octets in the Transfer Frame minus this (§4.1.2.7.2). */
#define TC_FRAME_LENGTH_OFFSET 1u

/**
 * @brief TC Transfer Frame types — Bypass/Control Command Flag combinations (§table 4-1).
 *
 * The remaining combination (Bypass=0, Control Command=1) is reserved for future
 * application and is rejected on decode.
 */
typedef enum
{
    SDLP_TC_FRAME_TYPE_AD = 0, /**< Bypass=0, CC=0: FDU, Sequence-Controlled (AD) service. */
    SDLP_TC_FRAME_TYPE_BD,     /**< Bypass=1, CC=0: FDU, Expedited (BD) service. */
    SDLP_TC_FRAME_TYPE_BC      /**< Bypass=1, CC=1: Control Commands for the FARM. */
} sdlp_tc_frame_type_t;

/**
 * @brief Control Commands carried by Type-BC frames (CCSDS 232.0-B-4 §4.1.3.3).
 * @{
 */
/** @brief Unlock: one 'all zeroes' octet (§4.1.3.3.2). */
#define TC_CONTROL_CMD_UNLOCK 0x00u
/** @brief Unlock command length in octets. */
#define TC_CONTROL_CMD_UNLOCK_LENGTH 1u
/** @brief Set V(R) octet 0: '10000010' (§4.1.3.3.3). */
#define TC_CONTROL_CMD_SET_VR_OCTET0 0x82u
/** @brief Set V(R) octet 1: '00000000'. */
#define TC_CONTROL_CMD_SET_VR_OCTET1 0x00u
/** @brief Set V(R) command length in octets. */
#define TC_CONTROL_CMD_SET_VR_LENGTH 3u
/** @} */

#ifdef TC_SEGMENT_HEADER_ENABLED

/** @brief Segment Header size in octets (§4.1.3.2.2). */
#    define TC_SEGMENT_HEADER_SIZE 1

/**
 * @brief TC Segment Header Sequence Flags (CCSDS 232.0-B-4 §table 4-2).
 *
 * Enum values equal the 2-bit wire pattern directly; bit 0 is the MSB.
 */
typedef enum
{
    TC_SEQ_FLAG_CONTINUE = 0x00u, /**< '00': continuing portion of an SDU on one MAP. */
    TC_SEQ_FLAG_FIRST = 0x01u,    /**< '01': first portion of an SDU on one MAP. */
    TC_SEQ_FLAG_LAST = 0x02u,     /**< '10': last portion of an SDU on one MAP. */
    TC_SEQ_FLAG_NO_SEG = 0x03u    /**< '11': no segmentation (complete SDU or multiple packets). */
} sdlp_tc_seq_flag_t;

/**
 * @brief TC Segment Header fields (CCSDS 232.0-B-4 §4.1.3.2.2).
 */
typedef struct
{
    uint8_t sequence_flags : 2; /**< Sequence Flags (see ::sdlp_tc_seq_flag_t). */
    uint8_t map_id : 6;         /**< Multiplexer Access Point (MAP) Identifier (0-63). */
} sdlp_tc_segment_header_t;

#endif /* TC_SEGMENT_HEADER_ENABLED */

/**
 * @brief TC Transfer Frame Primary Header fields (CCSDS 232.0-B-4 §4.1.2).
 *
 * @note Do not serialise this struct directly; use sdlp_tc_encode_frame().
 */
typedef struct
{
    uint16_t transfer_frame_version : 2; /**< Transfer Frame Version Number ('00', §4.1.2.2). */
    uint16_t bypass_flag : 1;            /**< Bypass Flag (Type-A=0 / Type-B=1, §4.1.2.3.1). */
    uint16_t control_command_flag : 1;   /**< Control Command Flag (Type-D=0 / Type-C=1). */
    uint16_t reserved : 2;               /**< Reserved Spare ('00', §4.1.2.4). */
    uint16_t spacecraft_id : 10;         /**< Spacecraft Identifier (10 bits). */
    uint16_t virtual_channel_id : 6;     /**< Virtual Channel Identifier (6 bits). */
    uint16_t frame_length : 10;          /**< Frame Length: total octets − 1 (§4.1.2.7). */
    uint8_t frame_sequence_number;       /**< Frame Sequence Number N(S) (§4.1.2.8). */
} sdlp_tc_header_t;

/**
 * @brief A TC Transfer Frame: header, optional Segment Header, and Data Field.
 *
 * @note Do not serialise this struct directly; use sdlp_tc_encode_frame().
 */
typedef struct
{
    sdlp_tc_header_t header; /**< Primary header fields. */
#ifdef TC_SEGMENT_HEADER_ENABLED
    sdlp_tc_segment_header_t segment_header; /**< Segment Header (Type-D frames only). */
#endif
    uint8_t data[TC_MAX_DATA_SIZE]; /**< Transfer Frame Data Field. */
    uint16_t data_length;           /**< Data Field length in octets. */
    uint16_t fecf;                  /**< Frame Error Control Field (application-managed). */
} sdlp_tc_frame_t;

/**
 * @brief Build a Type-AD TC Transfer Frame carrying a Data Field.
 *
 * @param[out] frame              Target frame.
 * @param[in]  spacecraft_id      Spacecraft Identifier — masked to 10 bits.
 * @param[in]  virtual_channel_id Virtual Channel Identifier — masked to 6 bits.
 * @param[in]  frame_seq_num      Frame Sequence Number N(S).
 * @param[in]  data               Data Field content (copied into the frame).
 * @param[in]  data_length        Data Field length (1..::TC_MAX_DATA_SIZE, one less with a
 *                                Segment Header).
 * @return ::SDLP_SUCCESS, or ::SDLP_ERROR_INVALID_PARAM on NULL args or bad length.
 */
sdlp_status_t sdlp_tc_create_frame(sdlp_tc_frame_t *frame,
                                   uint16_t spacecraft_id,
                                   uint8_t virtual_channel_id,
                                   uint8_t frame_seq_num,
                                   const uint8_t *data,
                                   uint16_t data_length);

/**
 * @brief Select the Transfer Frame type by setting the Bypass/Control Command Flags (§4.1.2.3).
 *
 * Also recomputes the Frame Length, since Type-BC frames carry no Segment Header
 * (§4.1.3.2.2.1.3). sdlp_tc_create_frame() produces a Type-AD frame.
 *
 * @param[in,out] frame Target frame.
 * @param[in]     type  Desired frame type.
 * @return ::SDLP_SUCCESS, or ::SDLP_ERROR_INVALID_PARAM on a NULL frame or unknown type.
 */
sdlp_status_t sdlp_tc_set_frame_type(sdlp_tc_frame_t *frame, sdlp_tc_frame_type_t type);

/**
 * @brief Build a complete Type-BC frame carrying the Unlock Control Command (§4.1.3.3.2).
 *
 * The Frame Sequence Number is set to zero (COP does not use it for Type-B frames).
 *
 * @param[out] frame              Target frame.
 * @param[in]  spacecraft_id      Spacecraft Identifier — masked to 10 bits.
 * @param[in]  virtual_channel_id Virtual Channel Identifier — masked to 6 bits.
 * @return ::SDLP_SUCCESS, or ::SDLP_ERROR_INVALID_PARAM on a NULL frame.
 */
sdlp_status_t sdlp_tc_create_unlock_frame(sdlp_tc_frame_t *frame,
                                          uint16_t spacecraft_id,
                                          uint8_t virtual_channel_id);

/**
 * @brief Build a complete Type-BC frame carrying the Set V(R) Control Command (§4.1.3.3.3).
 *
 * @param[out] frame              Target frame.
 * @param[in]  spacecraft_id      Spacecraft Identifier — masked to 10 bits.
 * @param[in]  virtual_channel_id Virtual Channel Identifier — masked to 6 bits.
 * @param[in]  vr                 Value the FARM should load into Receiver_Frame_Sequence_Number.
 * @return ::SDLP_SUCCESS, or ::SDLP_ERROR_INVALID_PARAM on a NULL frame.
 */
sdlp_status_t sdlp_tc_create_set_vr_frame(sdlp_tc_frame_t *frame,
                                          uint16_t spacecraft_id,
                                          uint8_t virtual_channel_id,
                                          uint8_t vr);

/**
 * @brief Serialise a TC Transfer Frame into a caller-supplied buffer.
 *
 * Emits the primary header, the Segment Header (when compiled in and the frame is
 * not Type-BC), the Data Field, then the FECF (verbatim from @p frame->fecf). The
 * wire Frame Length is derived from the emitted octet count. On failure, @p buffer
 * and @p encoded_size are left unchanged.
 *
 * @param[in]  frame        Frame to serialise.
 * @param[out] buffer       Output buffer.
 * @param[in]  buffer_size  Buffer capacity in octets.
 * @param[out] encoded_size Bytes written on success.
 * @return ::SDLP_SUCCESS; ::SDLP_ERROR_INVALID_PARAM on NULL args or a frame that would
 *         exceed ::TC_MAX_FRAME_SIZE octets; ::SDLP_ERROR_BUFFER_TOO_SMALL if @p buffer
 *         cannot hold the frame.
 */
sdlp_status_t sdlp_tc_encode_frame(const sdlp_tc_frame_t *frame,
                                   uint8_t *buffer,
                                   size_t buffer_size,
                                   size_t *encoded_size);

/**
 * @brief Parse a wire-format TC Transfer Frame.
 *
 * Validates that the Frame Length matches the octet count and rejects the reserved
 * Bypass=0/Control Command=1 combination. The FECF is surfaced without validation.
 * On failure, @p frame is left unchanged.
 *
 * @param[in]  buffer      Wire buffer to parse.
 * @param[in]  buffer_size Buffer length in octets.
 * @param[out] frame       Decoded frame.
 * @return ::SDLP_SUCCESS, ::SDLP_ERROR_INVALID_PARAM, or ::SDLP_ERROR_INVALID_FRAME.
 */
sdlp_status_t sdlp_tc_decode_frame(const uint8_t *buffer,
                                   size_t buffer_size,
                                   sdlp_tc_frame_t *frame);

#ifdef TC_SEGMENT_HEADER_ENABLED
/**
 * @brief Set the Segment Header fields on a TC frame (CCSDS 232.0-B-4 §4.1.3.2.2).
 *
 * The Segment Header must not be present in frames carrying Control Commands
 * (§4.1.3.2.2.1.3), so a frame with the Control Command Flag set is rejected. On failure,
 * @p frame is left unchanged.
 *
 * @param[in,out] frame          Target frame.
 * @param[in]     sequence_flags One of the ::sdlp_tc_seq_flag_t values.
 * @param[in]     map_id         Multiplexer Access Point Identifier (0-63).
 * @return ::SDLP_SUCCESS, or ::SDLP_ERROR_INVALID_PARAM on a NULL frame or a frame with the
 *         Control Command Flag set.
 */
sdlp_status_t sdlp_tc_set_segment_header(sdlp_tc_frame_t *frame,
                                         sdlp_tc_seq_flag_t sequence_flags,
                                         uint8_t map_id);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SDLP_TC_H */
