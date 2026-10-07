/**
 * @file sdlp_tc.c
 * @brief TC Space Data Link Protocol frame handling (CCSDS 232.0-B-4).
 *
 * Implements the API declared in sdlp_tc.h: create/encode/decode of TC Transfer
 * Frames, the AD/BD/BC frame-type selector, and the Unlock / Set V(R) control-
 * command builders.
 */
#include "sdlp_tc.h"

#include <string.h>

/**
 * @brief Compute the wire Frame Length: total octets in the Transfer Frame − 1 (§4.1.2.7.2).
 *
 * The Segment Header, when compiled in, is carried only by frames conveying Frame
 * Data Units, never by Type-BC (control command) frames (§4.1.3.2.2.1.3).
 *
 * @param[in] frame Frame whose current fields determine the emitted size.
 * @return The 10-bit Frame Length value.
 */
static uint16_t tc_frame_length(const sdlp_tc_frame_t *frame)
{
    size_t frame_octets =
        (size_t)TC_PRIMARY_HEADER_SIZE + frame->data_length + TC_FRAME_ERROR_CONTROL_SIZE;
#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag)
    {
        frame_octets += TC_SEGMENT_HEADER_SIZE;
    }
#endif
    return (uint16_t)(frame_octets - 1u);
}

/**
 * @brief Decode the 5-octet Transfer Frame Primary Header (CCSDS 232.0-B-4 §4.1.2).
 *
 * @param[in]  buffer Wire buffer holding at least ::TC_PRIMARY_HEADER_SIZE octets.
 * @param[out] header Decoded primary header fields.
 */
static void sdlp_tc_decode_primary_header(const uint8_t *buffer, sdlp_tc_header_t *header)
{
    memset(header, 0, sizeof(sdlp_tc_header_t));

    header->transfer_frame_version = (uint16_t)((buffer[0] >> 6) & 0x03u);
    header->bypass_flag = (uint16_t)((buffer[0] >> 5) & 0x01u);
    header->control_command_flag = (uint16_t)((buffer[0] >> 4) & 0x01u);
    header->reserved = (uint16_t)((buffer[0] >> 2) & 0x03u);
    header->spacecraft_id = (uint16_t)((((unsigned)buffer[0] << 8) | buffer[1]) & 0x03FFu);
    header->virtual_channel_id = (uint16_t)((buffer[2] >> 2) & 0x3Fu);
    header->frame_length = (uint16_t)((((unsigned)buffer[2] << 8) | buffer[3]) & 0x03FFu);
    header->frame_sequence_number = buffer[4];
}

sdlp_status_t sdlp_tc_create_frame(sdlp_tc_frame_t *frame,
                                   uint16_t spacecraft_id,
                                   uint8_t virtual_channel_id,
                                   uint8_t frame_seq_num,
                                   const uint8_t *data,
                                   uint16_t data_length)
{
    /* A Type-D Frame Data Unit carries a Segment Header (when configured), which
     * consumes one octet of the Data Field budget (CCSDS 232.0-B-4, 4.1.3.2.1). */
    size_t max_data = TC_MAX_DATA_SIZE;
#ifdef TC_SEGMENT_HEADER_ENABLED
    max_data -= TC_SEGMENT_HEADER_SIZE;
#endif

    if ((!frame) || (!data) || (data_length == 0u) || (data_length > max_data))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    memset(frame, 0, sizeof(sdlp_tc_frame_t));

    frame->header.transfer_frame_version = SDLP_VERSION;
    frame->header.bypass_flag = 0;
    frame->header.control_command_flag = 0;
    frame->header.reserved = 0;
    frame->header.spacecraft_id = (uint16_t)(spacecraft_id & 0x3FFu);
    frame->header.virtual_channel_id = (uint8_t)(virtual_channel_id & 0x3Fu);
    frame->header.frame_sequence_number = frame_seq_num;

    memcpy(frame->data, data, data_length);
    frame->data_length = data_length;

    /* sdlp_tc_encode_frame recomputes the Frame Length from the bytes it actually
     * emits; it is set here so the struct is self-consistent. */
    frame->header.frame_length = (uint16_t)(tc_frame_length(frame) & 0x03FFu);

    return SDLP_SUCCESS;
}

sdlp_status_t sdlp_tc_set_frame_type(sdlp_tc_frame_t *frame, sdlp_tc_frame_type_t type)
{
    if (!frame)
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    switch (type)
    {
    case SDLP_TC_FRAME_TYPE_AD:
        frame->header.bypass_flag = 0;
        frame->header.control_command_flag = 0;
        break;
    case SDLP_TC_FRAME_TYPE_BD:
        frame->header.bypass_flag = 1;
        frame->header.control_command_flag = 0;
        break;
    case SDLP_TC_FRAME_TYPE_BC:
        frame->header.bypass_flag = 1;
        frame->header.control_command_flag = 1;
        break;
    default:
        return SDLP_ERROR_INVALID_PARAM;
    }

    /* The Segment Header is absent from Type-BC frames, so the frame type affects
     * the total frame size when segment headers are compiled in. */
    frame->header.frame_length = (uint16_t)(tc_frame_length(frame) & 0x03FFu);

    return SDLP_SUCCESS;
}

sdlp_status_t sdlp_tc_create_unlock_frame(sdlp_tc_frame_t *frame,
                                          uint16_t spacecraft_id,
                                          uint8_t virtual_channel_id)
{
    const uint8_t cmd[TC_CONTROL_CMD_UNLOCK_LENGTH] = {TC_CONTROL_CMD_UNLOCK};

    /* COP does not use the Frame Sequence Number of Type-B frames; it is set to
     * 'all zeroes' (CCSDS 232.0-B-4, 4.1.2.8 note 3). */
    sdlp_status_t result = sdlp_tc_create_frame(frame,
                                                spacecraft_id,
                                                virtual_channel_id,
                                                0,
                                                cmd,
                                                (uint16_t)sizeof(cmd));
    if (result != SDLP_SUCCESS)
    {
        return result;
    }

    return sdlp_tc_set_frame_type(frame, SDLP_TC_FRAME_TYPE_BC);
}

sdlp_status_t sdlp_tc_create_set_vr_frame(sdlp_tc_frame_t *frame,
                                          uint16_t spacecraft_id,
                                          uint8_t virtual_channel_id,
                                          uint8_t vr)
{
    const uint8_t cmd[TC_CONTROL_CMD_SET_VR_LENGTH] = {TC_CONTROL_CMD_SET_VR_OCTET0,
                                                       TC_CONTROL_CMD_SET_VR_OCTET1,
                                                       vr};

    /* COP does not use the Frame Sequence Number of Type-B frames; it is set to
     * 'all zeroes' (CCSDS 232.0-B-4, 4.1.2.8 note 3). */
    sdlp_status_t result = sdlp_tc_create_frame(frame,
                                                spacecraft_id,
                                                virtual_channel_id,
                                                0,
                                                cmd,
                                                (uint16_t)sizeof(cmd));
    if (result != SDLP_SUCCESS)
    {
        return result;
    }

    return sdlp_tc_set_frame_type(frame, SDLP_TC_FRAME_TYPE_BC);
}

sdlp_status_t sdlp_tc_encode_frame(const sdlp_tc_frame_t *frame,
                                   uint8_t *buffer,
                                   size_t buffer_size,
                                   size_t *encoded_size)
{
    if ((!frame) || (!buffer) || (!encoded_size))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    size_t required_size =
        (size_t)TC_PRIMARY_HEADER_SIZE + frame->data_length + TC_FRAME_ERROR_CONTROL_SIZE;

#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag)
    {
        required_size += TC_SEGMENT_HEADER_SIZE;
    }
#endif

    /* A Transfer Frame is at most TC_MAX_FRAME_SIZE octets, the most the 10-bit Frame
     * Length can express (CCSDS 232.0-B-4, 4.1.2.7). The same limit keeps data_length
     * within frame->data, so the copy below cannot read past the array. */
    if (required_size > TC_MAX_FRAME_SIZE)
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    if (buffer_size < required_size)
    {
        return SDLP_ERROR_BUFFER_TOO_SMALL;
    }

    /* Frame Length = total octets in the emitted Transfer Frame - 1 (CCSDS 232.0-B-4,
     * 4.1.2.7.2), derived from the actual encoded size so it always matches the wire. */
    uint16_t frame_length = (uint16_t)(required_size - 1u);

    size_t offset = 0;

    buffer[offset++] = (uint8_t)(((frame->header.transfer_frame_version & 0x03u) << 6) |
                                 ((frame->header.bypass_flag & 0x01u) << 5) |
                                 ((frame->header.control_command_flag & 0x01u) << 4) |
                                 ((frame->header.reserved & 0x03u) << 2) |
                                 ((frame->header.spacecraft_id >> 8) & 0x03u));
    buffer[offset++] = (uint8_t)(frame->header.spacecraft_id & 0xFFu);
    buffer[offset++] = (uint8_t)(((frame->header.virtual_channel_id & 0x3Fu) << 2) |
                                 ((frame_length >> 8) & 0x03u));
    buffer[offset++] = (uint8_t)(frame_length & 0xFFu);
    buffer[offset++] = frame->header.frame_sequence_number;

#ifdef TC_SEGMENT_HEADER_ENABLED
    if (!frame->header.control_command_flag)
    {
        buffer[offset++] = (uint8_t)(((frame->segment_header.sequence_flags & 0x03u) << 6) |
                                     (frame->segment_header.map_id & 0x3Fu));
    }
#endif

    memcpy(&buffer[offset], frame->data, frame->data_length);
    offset += frame->data_length;

    /* The Frame Error Control Field is passed through verbatim; computing an
     * error-control value (e.g. CRC-16) is left to the application. */
    buffer[offset++] = (uint8_t)((frame->fecf >> 8) & 0xFFu);
    buffer[offset++] = (uint8_t)(frame->fecf & 0xFFu);

    *encoded_size = offset;

    return SDLP_SUCCESS;
}

sdlp_status_t sdlp_tc_decode_frame(const uint8_t *buffer,
                                   size_t buffer_size,
                                   sdlp_tc_frame_t *frame)
{
    if ((!buffer) || (!frame) ||
        (buffer_size < (TC_PRIMARY_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE)))
    {
        return SDLP_ERROR_INVALID_PARAM;
    }

    sdlp_tc_header_t header;
    sdlp_tc_decode_primary_header(buffer, &header);

    /* Bypass=0 with Control Command=1 is reserved for future application
     * (CCSDS 232.0-B-4, table 4-1). */
    if ((!header.bypass_flag) && (header.control_command_flag))
    {
        return SDLP_ERROR_INVALID_FRAME;
    }

    /* Frame Validation: the Frame Length must equal the actual octet count minus one
     * (CCSDS 232.0-B-4, 4.1.2.7.2). */
    if (((size_t)header.frame_length + 1u) != buffer_size)
    {
        return SDLP_ERROR_INVALID_FRAME;
    }

    size_t data_offset = TC_PRIMARY_HEADER_SIZE;

#ifdef TC_SEGMENT_HEADER_ENABLED
    sdlp_tc_segment_header_t segment_header = {0};
    if (!header.control_command_flag)
    {
        if (buffer_size <
            (TC_PRIMARY_HEADER_SIZE + TC_SEGMENT_HEADER_SIZE + TC_FRAME_ERROR_CONTROL_SIZE))
        {
            return SDLP_ERROR_INVALID_FRAME;
        }
        segment_header.sequence_flags = (buffer[data_offset] >> 6) & 0x03u;
        segment_header.map_id = buffer[data_offset] & 0x3Fu;
        data_offset += TC_SEGMENT_HEADER_SIZE;
    }
#endif

    /* The Frame Length validation above pins buffer_size to frame_length + 1 (<= 1024),
     * so data_length is inherently <= TC_MAX_DATA_SIZE and the Data Field fits. */
    const size_t data_length = buffer_size - data_offset - TC_FRAME_ERROR_CONTROL_SIZE;
    const size_t fecf_offset = data_offset + data_length;

    /* Every check has passed. The output frame is written only from here on, so a
     * rejected buffer leaves it exactly as the caller passed it. */
    memset(frame, 0, sizeof(sdlp_tc_frame_t));
    frame->header = header;
#ifdef TC_SEGMENT_HEADER_ENABLED
    frame->segment_header = segment_header;
#endif
    memcpy(frame->data, &buffer[data_offset], data_length);
    frame->data_length = (uint16_t)data_length;

    /* The Frame Error Control Field is surfaced as-is; validating it (e.g. via
     * CRC-16) is left to the application. */
    frame->fecf = (uint16_t)(((uint16_t)buffer[fecf_offset] << 8) | buffer[fecf_offset + 1u]);

    return SDLP_SUCCESS;
}

#ifdef TC_SEGMENT_HEADER_ENABLED
sdlp_status_t sdlp_tc_set_segment_header(sdlp_tc_frame_t *frame,
                                         sdlp_tc_seq_flag_t sequence_flags,
                                         uint8_t map_id)
{
    if (!frame)
    {
        return SDLP_ERROR_INVALID_PARAM;
    }
    frame->segment_header.sequence_flags = (uint8_t)(sequence_flags & 0x03u);
    frame->segment_header.map_id = (uint8_t)(map_id & 0x3Fu);
    return SDLP_SUCCESS;
}
#endif
