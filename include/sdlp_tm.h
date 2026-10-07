/**
 * @file sdlp_tm.h
 * @brief CCSDS TM Space Data Link Protocol frame handling (CCSDS 132.0-B-3).
 *
 * Builds, encodes, and decodes TM Transfer Frames: the 6-octet primary header,
 * the optional Transfer Frame Secondary Header, the Data Field, the optional
 * Operational Control Field, and the 2-octet Frame Error Control Field. Data
 * Field, Secondary Header, OCF, and FECF content are treated as opaque,
 * application-owned octet sequences.
 */

#ifndef SDLP_TM_H
#define SDLP_TM_H

#include "sdlp_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TM_PRIMARY_HEADER_SIZE 6      /**< Transfer Frame Primary Header size (§4.1.2). */
#define TM_FRAME_ERROR_CONTROL_SIZE 2 /**< Frame Error Control Field size (§4.1.6). */
#define TM_MAX_DATA_SIZE 1024         /**< Maximum Transfer Frame Data Field carried. */

/**
 * @brief Transfer Frame Secondary Header sizing (CCSDS 132.0-B-3 §4.1.3).
 *
 * A 1-octet Identification Field followed by a Data Field of 1 to 63 octets,
 * i.e. up to 64 octets in total.
 * @{
 */
#define TM_SECONDARY_HEADER_ID_SIZE 1   /**< Identification Field size in octets. */
#define TM_SECONDARY_HEADER_MIN_DATA 1  /**< Minimum Secondary Header Data Field length. */
#define TM_SECONDARY_HEADER_MAX_DATA 63 /**< Maximum Secondary Header Data Field length. */
/** @} */

#define TM_OCF_SIZE 4 /**< Operational Control Field size in octets (§4.1.5). */

/**
 * @brief Number of distinct Master Channels whose frame counts are tracked concurrently.
 *
 * TM Master/Virtual Channel Frame Counts are kept per Master Channel (identified by
 * Spacecraft ID, as the Transfer Frame Version Number is fixed at 0) and, within each,
 * per Virtual Channel (CCSDS 132.0-B-3 §4.1.2.5 and §4.1.2.6). The state lives in a
 * fixed-size table (no dynamic allocation), so sdlp_tm_create_frame() returns
 * ::SDLP_ERROR_NO_RESOURCE for a Spacecraft ID beyond this many. Define it when building
 * the library to change the limit.
 */
#ifndef TM_MAX_MASTER_CHANNELS
#    define TM_MAX_MASTER_CHANNELS 8
#endif

/**
 * @brief Transfer Frame Data Field Status sub-field values (CCSDS 132.0-B-3 §4.1.2.7).
 * @{
 */
/** '11'; mandatory Segment Length Identifier when Sync Flag = 0 (§4.1.2.7.5.2). */
#define TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION 0x03u
/** First Header Pointer: no Packet starts in the Data Field (§4.1.2.7.6.4). */
#define TM_FIRST_HEADER_POINTER_NO_PACKET 0x07FFu
/** First Header Pointer: Only Idle Data (OID) Transfer Frame (§4.1.2.7.6.5). */
#define TM_FIRST_HEADER_POINTER_ONLY_IDLE 0x07FEu
/** @} */

/**
 * @brief Transfer Frame Data Field Status fields (CCSDS 132.0-B-3 §4.1.2.7).
 *
 * Occupies 2 octets on the wire, most significant bit first: secondary header
 * flag (1) | synchronization flag (1) | packet order flag (1) | segment length
 * identifier (2) | first header pointer (11). Convert to/from the wire value with
 * ::sdlp_tm_pack_data_field_status and ::sdlp_tm_unpack_data_field_status.
 */
typedef struct
{
    uint16_t secondary_header_flag : 1; /**< Presence of the Transfer Frame Secondary Header. */
    uint16_t sync_flag : 1;             /**< 0 = Packets/Idle Data, 1 = VCA_SDU. */
    uint16_t packet_order_flag : 1;     /**< Reserved ('0') when sync_flag = 0. */
    uint16_t segment_length_id : 2;     /**< 2 bits; '11' when sync_flag = 0. */
    uint16_t first_header_pointer : 11; /**< Offset of the first Packet, or a special value. */
} sdlp_tm_data_field_status_t;

/**
 * @brief TM Transfer Frame Primary Header fields (CCSDS 132.0-B-3 §4.1.2).
 *
 * @note Do not serialise this struct directly; use sdlp_tm_encode_frame().
 */
typedef struct
{
    uint16_t transfer_frame_version : 2; /**< Transfer Frame Version Number ('00', §4.1.2.2.2). */
    uint16_t spacecraft_id : 10;         /**< Spacecraft Identifier (10 bits). */
    uint16_t virtual_channel_id : 3;     /**< Virtual Channel Identifier (3 bits). */
    uint16_t ocf_flag : 1;               /**< Operational Control Field Flag. */
    uint8_t master_channel_frame_count;  /**< Master Channel Frame Count, modulo-256 (§4.1.2.5). */
    uint8_t virtual_channel_frame_count; /**< Virtual Channel Frame Count, modulo-256 (§4.1.2.6). */
    sdlp_tm_data_field_status_t transfer_frame_data_field_status; /**< Data Field Status. */
} sdlp_tm_header_t;

/**
 * @brief Transfer Frame Secondary Header (CCSDS 132.0-B-3 §4.1.3).
 *
 * The 1-octet Identification Field carries a 2-bit Version Number ('00') and a
 * 6-bit Length. The wire Length equals the total Secondary Header size minus one,
 * which — the Identification Field being one octet — is exactly the Data Field
 * length. Attach one with sdlp_tm_set_secondary_header().
 */
typedef struct
{
    uint8_t version : 2;                        /**< Secondary Header Version Number ('00'). */
    uint8_t length : 6;                         /**< Data Field length (1..63); 0 = absent. */
    uint8_t data[TM_SECONDARY_HEADER_MAX_DATA]; /**< Secondary Header Data Field. */
} sdlp_tm_secondary_header_t;

/**
 * @brief A TM Transfer Frame: header, optional trailers, and Data Field.
 *
 * @note Do not serialise this struct directly; use sdlp_tm_encode_frame().
 */
typedef struct
{
    sdlp_tm_header_t header;                     /**< Primary header fields. */
    sdlp_tm_secondary_header_t secondary_header; /**< Secondary Header (when its flag is set). */
    uint8_t data[TM_MAX_DATA_SIZE];              /**< Transfer Frame Data Field. */
    uint16_t data_length;                        /**< Data Field length in octets. */
    uint8_t ocf[TM_OCF_SIZE];                    /**< OCF (valid when ocf_flag is set). */
    uint16_t fecf;                               /**< Frame Error Control Field (app-managed). */
} sdlp_tm_frame_t;

/**
 * @brief Pack a Transfer Frame Data Field Status into its 2-octet wire value.
 *
 * @param[in] status Fields to pack.
 * @return The 16-bit wire value, or 0 if @p status is NULL.
 */
uint16_t sdlp_tm_pack_data_field_status(const sdlp_tm_data_field_status_t *status);

/**
 * @brief Parse a 2-octet Transfer Frame Data Field Status into its sub-fields.
 *
 * @param[in]  raw    16-bit wire value.
 * @param[out] status Decoded fields. No-op if NULL.
 */
void sdlp_tm_unpack_data_field_status(uint16_t raw, sdlp_tm_data_field_status_t *status);

/**
 * @brief Build a TM Transfer Frame carrying a Data Field.
 *
 * Sets a standards-valid default Data Field Status (Sync Flag = 0, Segment Length
 * Identifier '11') and advances the per-Master/Virtual-Channel frame counts. On failure,
 * @p frame is left unchanged and no frame count advances.
 *
 * @param[out] frame              Target frame.
 * @param[in]  spacecraft_id      Spacecraft Identifier — masked to 10 bits.
 * @param[in]  virtual_channel_id Virtual Channel Identifier — masked to 3 bits.
 * @param[in]  data               Data Field content (copied into the frame).
 * @param[in]  data_length        Data Field length (0..::TM_MAX_DATA_SIZE).
 * @return ::SDLP_SUCCESS; ::SDLP_ERROR_INVALID_PARAM on NULL args or oversized data;
 *         ::SDLP_ERROR_NO_RESOURCE if @p spacecraft_id is a new Master Channel and
 *         ::TM_MAX_MASTER_CHANNELS of them are already tracked.
 */
sdlp_status_t sdlp_tm_create_frame(sdlp_tm_frame_t *frame,
                                   uint16_t spacecraft_id,
                                   uint8_t virtual_channel_id,
                                   const uint8_t *data,
                                   uint16_t data_length);

/**
 * @brief Forget every Master Channel and restart all frame counts from zero.
 *
 * Empties the table sdlp_tm_create_frame() uses to number frames, freeing all
 * ::TM_MAX_MASTER_CHANNELS slots. Meant for (re-)initialisation and for tests: in normal
 * operation a count must not be reset unless it is unavoidable (CCSDS 132.0-B-3
 * §4.1.2.5.3 and §4.1.2.6.3). Not thread-safe.
 */
void sdlp_tm_reset_frame_counts(void);

/**
 * @brief Attach a Transfer Frame Secondary Header (CCSDS 132.0-B-3 §4.1.3).
 *
 * Raises the Secondary Header Flag and copies @p length octets of Data Field
 * content; the Version Number is set to '00'.
 *
 * @param[out] frame  Target frame.
 * @param[in]  data   Secondary Header Data Field content (copied).
 * @param[in]  length Data Field length (1..::TM_SECONDARY_HEADER_MAX_DATA).
 * @return ::SDLP_SUCCESS, or ::SDLP_ERROR_INVALID_PARAM on NULL args or bad length.
 */
sdlp_status_t sdlp_tm_set_secondary_header(sdlp_tm_frame_t *frame,
                                           const uint8_t *data,
                                           uint8_t length);

/**
 * @brief Attach a 4-octet Operational Control Field (CCSDS 132.0-B-3 §4.1.5).
 *
 * Raises the OCF Flag and copies the OCF content verbatim. The content (e.g. a
 * CLCW or an SDLS report) is mission-specific. Leave this unset to emit a frame
 * with no OCF.
 *
 * @param[out] frame Target frame.
 * @param[in]  ocf   Four octets of OCF content (copied).
 * @return ::SDLP_SUCCESS, or ::SDLP_ERROR_INVALID_PARAM on NULL args.
 */
sdlp_status_t sdlp_tm_set_ocf(sdlp_tm_frame_t *frame, const uint8_t ocf[TM_OCF_SIZE]);

/**
 * @brief Serialise a TM Transfer Frame into a caller-supplied buffer.
 *
 * Emits, in order: primary header, Secondary Header (if flagged), Data Field,
 * OCF (if flagged), then the FECF (written verbatim from @p frame->fecf). On failure,
 * @p buffer and @p encoded_size are left unchanged.
 *
 * @param[in]  frame        Frame to serialise.
 * @param[out] buffer       Output buffer.
 * @param[in]  buffer_size  Buffer capacity in octets.
 * @param[out] encoded_size Bytes written on success.
 * @return ::SDLP_SUCCESS; ::SDLP_ERROR_INVALID_PARAM on NULL args, a Data Field longer
 *         than ::TM_MAX_DATA_SIZE, or the Secondary Header Flag set with an empty Secondary
 *         Header; ::SDLP_ERROR_BUFFER_TOO_SMALL if @p buffer cannot hold the frame.
 */
sdlp_status_t sdlp_tm_encode_frame(const sdlp_tm_frame_t *frame,
                                   uint8_t *buffer,
                                   size_t buffer_size,
                                   size_t *encoded_size);

/**
 * @brief Parse a wire-format TM Transfer Frame.
 *
 * Recovers the header, any Secondary Header / OCF signalled by their flags, the
 * Data Field, and the FECF (surfaced as-is, without validation). On failure,
 * @p frame is left unchanged.
 *
 * @param[in]  buffer      Wire buffer to parse.
 * @param[in]  buffer_size Buffer length in octets.
 * @param[out] frame       Decoded frame.
 * @return ::SDLP_SUCCESS, ::SDLP_ERROR_INVALID_PARAM, or ::SDLP_ERROR_INVALID_FRAME.
 */
sdlp_status_t sdlp_tm_decode_frame(const uint8_t *buffer,
                                   size_t buffer_size,
                                   sdlp_tm_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* SDLP_TM_H */
