/**
 * @file sdlp_tm.c
 * @brief TM Space Data Link Protocol frame handling (CCSDS 132.0-B-3).
 *
 * Implements the API declared in sdlp_tm.h: create/encode/decode of TM Transfer
 * Frames, the Data Field Status codec, the Secondary Header and OCF setters, and
 * the per-Master/Virtual-Channel frame counters.
 */
#include "sdlp_tm.h"

#include <string.h>

#define TM_VC_PER_MC 8 /**< TM Virtual Channels per Master Channel (VCID is 3 bits). */

/**
 * @brief Per-Master-Channel frame-count state.
 */
typedef struct
{
    uint8_t in_use;                       /**< Slot occupied. */
    uint16_t spacecraft_id;               /**< Master Channel ID (Spacecraft Identifier). */
    uint8_t mc_frame_count;               /**< Master Channel Frame Count (modulo-256). */
    uint8_t vc_frame_count[TM_VC_PER_MC]; /**< Virtual Channel Frame Counts, indexed by VCID. */
} tm_master_channel_t;

/**
 * @brief Fixed-size table of per-Master-Channel counter state (see ::TM_MAX_MASTER_CHANNELS).
 *
 * Both counts are free-running modulo-256. Not thread-safe.
 */
static tm_master_channel_t tm_master_channels[TM_MAX_MASTER_CHANNELS];

/**
 * @brief Return the counter state for a Master Channel, allocating a slot on first use.
 *
 * @param[in] spacecraft_id Master Channel ID (Spacecraft Identifier) to look up.
 * @return Pointer to the channel's counters, or NULL if the table is already full of
 *         other Master Channels.
 */
static tm_master_channel_t *tm_get_master_channel(uint16_t spacecraft_id)
{
    for (size_t i = 0; i < TM_MAX_MASTER_CHANNELS; i++)
    {
        if ((tm_master_channels[i].in_use) &&
            (tm_master_channels[i].spacecraft_id == spacecraft_id))
        {
            return &tm_master_channels[i];
        }
    }
    for (size_t i = 0; i < TM_MAX_MASTER_CHANNELS; i++)
    {
        if (!tm_master_channels[i].in_use)
        {
            tm_master_channels[i].in_use = 1;
            tm_master_channels[i].spacecraft_id = spacecraft_id;
            tm_master_channels[i].mc_frame_count = 0;
            memset(tm_master_channels[i].vc_frame_count,
                   0,
                   sizeof(tm_master_channels[i].vc_frame_count));
            return &tm_master_channels[i];
        }
    }
    return NULL;
}

/**
 * @brief Decode the 6-octet Transfer Frame Primary Header (CCSDS 132.0-B-3 §4.1.2).
 *
 * @param[in]  buffer Wire buffer holding at least ::TM_PRIMARY_HEADER_SIZE octets.
 * @param[out] header Decoded primary header fields.
 */
static void sdlp_tm_decode_primary_header(const uint8_t *buffer, sdlp_tm_header_t *header)
{
    const uint16_t data_field_status = (uint16_t)(((uint16_t)buffer[4] << 8) | buffer[5]);

    memset(header, 0, sizeof(sdlp_tm_header_t));

    header->transfer_frame_version = (uint16_t)((buffer[0] >> 6) & 0x03u);
    header->spacecraft_id = (uint16_t)(((buffer[0] & 0x3Fu) << 4) | ((buffer[1] >> 4) & 0x0Fu));
    header->virtual_channel_id = (uint16_t)((buffer[1] >> 1) & 0x07u);
    header->ocf_flag = (uint16_t)(buffer[1] & 0x01u);
    header->master_channel_frame_count = buffer[2];
    header->virtual_channel_frame_count = buffer[3];
    sdlp_tm_unpack_data_field_status(data_field_status, &header->transfer_frame_data_field_status);
}

/**
 * @brief Validate the Transfer Frame Secondary Header of a wire frame and return its size.
 *
 * Reads nothing beyond the Identification Field, so it is safe on any buffer that holds
 * at least a primary header and a Frame Error Control Field.
 *
 * @param[in] buffer      Wire buffer holding the whole Transfer Frame.
 * @param[in] buffer_size Buffer length in octets.
 * @return Secondary Header size in octets (Identification Field plus Data Field), or 0 if
 *         the header is malformed or does not fit in the frame.
 */
static size_t sdlp_tm_secondary_header_wire_size(const uint8_t *buffer, size_t buffer_size)
{
    /* Need the Identification Field plus at least one Data Field octet (4.1.3.1.3). */
    if (buffer_size <
        (TM_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_ID_SIZE + 1u + TM_FRAME_ERROR_CONTROL_SIZE))
    {
        return 0;
    }

    /* The wire Length is the total Secondary Header size minus one, which is exactly the
     * Data Field length (4.1.3.2.3.2). */
    const size_t data_length = buffer[TM_PRIMARY_HEADER_SIZE] & 0x3Fu;
    const size_t header_size = TM_SECONDARY_HEADER_ID_SIZE + data_length;

    if ((data_length == 0u) ||
        (buffer_size < (TM_PRIMARY_HEADER_SIZE + header_size + TM_FRAME_ERROR_CONTROL_SIZE)))
    {
        return 0;
    }

    return header_size;
}

/**
 * @brief Decode a wire-format Transfer Frame Secondary Header (CCSDS 132.0-B-3 §4.1.3).
 *
 * @param[in]  wire             Secondary Header octets, starting at the Identification Field
 *                              and already validated by sdlp_tm_secondary_header_wire_size().
 * @param[out] secondary_header Decoded Secondary Header.
 */
static void sdlp_tm_decode_secondary_header(const uint8_t *wire,
                                            sdlp_tm_secondary_header_t *secondary_header)
{
    secondary_header->version = (wire[0] >> 6) & 0x03u;
    secondary_header->length = wire[0] & 0x3Fu;
    memcpy(secondary_header->data, &wire[TM_SECONDARY_HEADER_ID_SIZE], secondary_header->length);
}

/**
 * @brief Compute the encoded size of a TM Transfer Frame, validating the lengths it depends on.
 *
 * A frame filled in by hand, rather than through sdlp_tm_create_frame() and
 * sdlp_tm_set_secondary_header(), can carry lengths that cannot be encoded.
 *
 * @param[in] frame Frame to measure.
 * @return Encoded size in octets, or 0 if the Data Field is longer than ::TM_MAX_DATA_SIZE
 *         or the Secondary Header Flag is set with an empty Secondary Header.
 */
static size_t sdlp_tm_encoded_size(const sdlp_tm_frame_t *frame)
{
    size_t size = (size_t)TM_PRIMARY_HEADER_SIZE + frame->data_length + TM_FRAME_ERROR_CONTROL_SIZE;

    /* A data_length beyond the Data Field array would make the encoder read past it. */
    if (frame->data_length > TM_MAX_DATA_SIZE)
    {
        return 0;
    }

    if (frame->header.transfer_frame_data_field_status.secondary_header_flag)
    {
        /* The wire Length is the total Secondary Header size minus one (CCSDS 132.0-B-3,
         * 4.1.3.2.3.2). A Length of 0 would be an Identification Field with no Data Field behind
         * it, which is mandatory */
        if (frame->secondary_header.length == 0u)
        {
            return 0;
        }
        size += TM_SECONDARY_HEADER_ID_SIZE + frame->secondary_header.length;
    }

    if (frame->header.ocf_flag)
    {
        size += TM_OCF_SIZE;
    }

    return size;
}

uint16_t sdlp_tm_pack_data_field_status(const sdlp_tm_data_field_status_t *status)
{
    if (!status)
    {
        return 0;
    }
    return (uint16_t)(((uint16_t)(status->secondary_header_flag & 0x01u) << 15) |
                      ((uint16_t)(status->sync_flag & 0x01u) << 14) |
                      ((uint16_t)(status->packet_order_flag & 0x01u) << 13) |
                      ((uint16_t)(status->segment_length_id & 0x03u) << 11) |
                      ((uint16_t)(status->first_header_pointer & 0x07FFu)));
}

void sdlp_tm_unpack_data_field_status(uint16_t raw, sdlp_tm_data_field_status_t *status)
{
    if (!status)
    {
        return;
    }
    status->secondary_header_flag = (uint8_t)((raw >> 15) & 0x01u);
    status->sync_flag = (uint8_t)((raw >> 14) & 0x01u);
    status->packet_order_flag = (uint8_t)((raw >> 13) & 0x01u);
    status->segment_length_id = (uint8_t)((raw >> 11) & 0x03u);
    status->first_header_pointer = (uint16_t)(raw & 0x07FFu);
}

void sdlp_tm_reset_frame_counts(void)
{
    memset(tm_master_channels, 0, sizeof(tm_master_channels));
}

sdlp_status_t sdlp_tm_create_frame(sdlp_tm_frame_t *frame,
                                   uint16_t spacecraft_id,
                                   uint8_t virtual_channel_id,
                                   const uint8_t *data,
                                   uint16_t data_length)
{
    if ((!frame) || (!data) || (data_length > TM_MAX_DATA_SIZE))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    /* The lookup is the last check and claims a table slot only when it succeeds, so a
     * rejected call leaves the frame and every frame count as they were. */
    tm_master_channel_t *mc = tm_get_master_channel((uint16_t)(spacecraft_id & 0x3FFu));
    if (!mc)
    {
        return SDLP_ERROR_NO_RESOURCE;
    }

    memset(frame, 0, sizeof(sdlp_tm_frame_t));

    frame->header.transfer_frame_version = SDLP_VERSION;
    frame->header.spacecraft_id = (uint16_t)(spacecraft_id & 0x3FFu);
    frame->header.virtual_channel_id = (uint16_t)(virtual_channel_id & 0x07u);
    frame->header.ocf_flag = 0;
    frame->header.master_channel_frame_count = mc->mc_frame_count++;
    frame->header.virtual_channel_frame_count =
        mc->vc_frame_count[frame->header.virtual_channel_id]++;

    /* Default to a valid "Packets, no segmentation" Data Field Status: Sync Flag = 0
     * requires the Segment Length Identifier to be '11' (CCSDS 132.0-B-3, 4.1.2.7.5.2),
     * and the First Header Pointer marks a Packet starting at the first data octet. */
    frame->header.transfer_frame_data_field_status.secondary_header_flag = 0;
    frame->header.transfer_frame_data_field_status.sync_flag = 0;
    frame->header.transfer_frame_data_field_status.packet_order_flag = 0;
    frame->header.transfer_frame_data_field_status.segment_length_id =
        TM_SEGMENT_LENGTH_ID_NO_SEGMENTATION;
    frame->header.transfer_frame_data_field_status.first_header_pointer = 0;

    memcpy(frame->data, data, data_length);
    frame->data_length = data_length;

    return SDLP_SUCCESS;
}

sdlp_status_t sdlp_tm_set_secondary_header(sdlp_tm_frame_t *frame,
                                           const uint8_t *data,
                                           uint8_t length)
{
    if ((!frame) || (!data) || (length == 0u) || (length > TM_SECONDARY_HEADER_MAX_DATA))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    frame->header.transfer_frame_data_field_status.secondary_header_flag = 1;
    frame->secondary_header.version = 0; /* CCSDS 132.0-B-3, 4.1.3.2.2.2 */
    frame->secondary_header.length = (uint8_t)(length & 0x3Fu);
    memcpy(frame->secondary_header.data, data, length);

    return SDLP_SUCCESS;
}

sdlp_status_t sdlp_tm_set_ocf(sdlp_tm_frame_t *frame, const uint8_t ocf[TM_OCF_SIZE])
{
    if ((!frame) || (!ocf))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    frame->header.ocf_flag = 1;
    memcpy(frame->ocf, ocf, TM_OCF_SIZE);

    return SDLP_SUCCESS;
}

sdlp_status_t sdlp_tm_encode_frame(const sdlp_tm_frame_t *frame,
                                   uint8_t *buffer,
                                   size_t buffer_size,
                                   size_t *encoded_size)
{
    if ((!frame) || (!buffer) || (!encoded_size))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    const size_t required_size = sdlp_tm_encoded_size(frame);
    if (required_size == 0u)
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    if (buffer_size < required_size)
    {
        return SDLP_ERROR_BUFFER_TOO_SMALL;
    }

    size_t offset = 0;

    buffer[offset++] = (uint8_t)(((frame->header.transfer_frame_version & 0x03u) << 6) |
                                 ((frame->header.spacecraft_id >> 4) & 0x3Fu));
    buffer[offset++] = (uint8_t)(((frame->header.spacecraft_id & 0x0Fu) << 4) |
                                 ((frame->header.virtual_channel_id & 0x07u) << 1) |
                                 (frame->header.ocf_flag & 0x01u));
    buffer[offset++] = frame->header.master_channel_frame_count;
    buffer[offset++] = frame->header.virtual_channel_frame_count;

    uint16_t data_field_status =
        sdlp_tm_pack_data_field_status(&frame->header.transfer_frame_data_field_status);
    buffer[offset++] = (uint8_t)((data_field_status >> 8) & 0xFFu);
    buffer[offset++] = (uint8_t)(data_field_status & 0xFFu);

    /* Transfer Frame Secondary Header (CCSDS 132.0-B-3, 4.1.3): Identification Field
     * (Version '00' | Length = total size - 1 = Data Field length) then the Data Field. */
    if (frame->header.transfer_frame_data_field_status.secondary_header_flag)
    {
        buffer[offset++] = (uint8_t)(((frame->secondary_header.version & 0x03u) << 6) |
                                     (frame->secondary_header.length & 0x3Fu));
        memcpy(&buffer[offset], frame->secondary_header.data, frame->secondary_header.length);
        offset += frame->secondary_header.length;
    }

    memcpy(&buffer[offset], frame->data, frame->data_length);
    offset += frame->data_length;

    /* Operational Control Field (CCSDS 132.0-B-3, 4.1.5): four octets following the
     * Data Field, present when the OCF Flag is set. The content is caller-supplied. */
    if (frame->header.ocf_flag)
    {
        memcpy(&buffer[offset], frame->ocf, TM_OCF_SIZE);
        offset += TM_OCF_SIZE;
    }

    /* The Frame Error Control Field is passed through verbatim; computing an
     * error-control value (e.g. CRC-16) is left to the application. */
    buffer[offset++] = (uint8_t)((frame->fecf >> 8) & 0xFFu);
    buffer[offset++] = (uint8_t)(frame->fecf & 0xFFu);

    *encoded_size = offset;

    return SDLP_SUCCESS;
}

sdlp_status_t sdlp_tm_decode_frame(const uint8_t *buffer,
                                   size_t buffer_size,
                                   sdlp_tm_frame_t *frame)
{
    if ((!buffer) || (!frame) ||
        (buffer_size < (TM_PRIMARY_HEADER_SIZE + TM_FRAME_ERROR_CONTROL_SIZE)))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    sdlp_tm_header_t header;
    sdlp_tm_decode_primary_header(buffer, &header);

    /* Transfer Frame Secondary Header (CCSDS 132.0-B-3, 4.1.3), present when the
     * Secondary Header Flag is set. Its size is signaled in the Identification Field. */
    size_t secondary_header_size = 0;
    if (header.transfer_frame_data_field_status.secondary_header_flag)
    {
        secondary_header_size = sdlp_tm_secondary_header_wire_size(buffer, buffer_size);
        if (secondary_header_size == 0u)
        {
            return SDLP_ERROR_INVALID_FRAME;
        }
    }

    /* Operational Control Field (CCSDS 132.0-B-3, 4.1.5): four octets between the
     * Data Field and the Frame Error Control Field, present when the OCF Flag is set. */
    const size_t ocf_size = header.ocf_flag ? TM_OCF_SIZE : 0;
    const size_t overhead =
        TM_PRIMARY_HEADER_SIZE + secondary_header_size + ocf_size + TM_FRAME_ERROR_CONTROL_SIZE;

    /* The Data Field length is range-checked at full width: narrowing it to the 16-bit
     * data_length first would let an oversized buffer wrap round to a small, valid length. */
    if ((buffer_size < overhead) || ((buffer_size - overhead) > TM_MAX_DATA_SIZE))
    {
        return SDLP_ERROR_INVALID_FRAME;
    }
    const size_t data_length = buffer_size - overhead;

    /* Every check has passed. The output frame is written only from here on, so a
     * rejected buffer leaves it exactly as the caller passed it. */
    memset(frame, 0, sizeof(sdlp_tm_frame_t));
    frame->header = header;

    size_t offset = TM_PRIMARY_HEADER_SIZE;
    if (secondary_header_size > 0u)
    {
        sdlp_tm_decode_secondary_header(&buffer[offset], &frame->secondary_header);
        offset += secondary_header_size;
    }

    memcpy(frame->data, &buffer[offset], data_length);
    frame->data_length = (uint16_t)data_length;
    offset += data_length;

    if (header.ocf_flag)
    {
        memcpy(frame->ocf, &buffer[offset], TM_OCF_SIZE);
        offset += TM_OCF_SIZE;
    }

    /* The Frame Error Control Field is surfaced as-is; validating it (e.g. via
     * CRC-16) is left to the application. */
    frame->fecf = (uint16_t)(((uint16_t)buffer[offset] << 8) | buffer[offset + 1]);

    return SDLP_SUCCESS;
}
