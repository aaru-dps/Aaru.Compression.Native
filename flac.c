//
// Created by claunia on 20/10/21.
//

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "3rdparty/flac/include/FLAC/metadata.h"
#include "3rdparty/flac/include/FLAC/stream_decoder.h"
#include "3rdparty/flac/include/FLAC/stream_encoder.h"
#include "library.h"
#include "flac.h"

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder,
                                                   FLAC__byte                 buffer[],
                                                   size_t *                   bytes,
                                                   void *                     client_data);

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder,
                                                     const FLAC__Frame *        frame,
                                                     const FLAC__int32 *const   buffer[],
                                                     void *                     client_data);

static void error_callback(const FLAC__StreamDecoder *    decoder,
                           FLAC__StreamDecoderErrorStatus status,
                           void *                         client_data);

static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder,
                                                   FLAC__uint64 *             absolute_byte_offset,
                                                   void *                     client_data);

static size_t flac_decode_redbook(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size)
{
    FLAC__StreamDecoder *         decoder;
    FLAC__StreamDecoderInitStatus init_status;
    aaru_flac_ctx *               ctx = (aaru_flac_ctx *)malloc(sizeof(aaru_flac_ctx));
    size_t                        ret_size;

    memset(ctx, 0, sizeof(aaru_flac_ctx));

    ctx->src_buffer = src_buffer;
    ctx->src_len    = src_size;
    ctx->src_pos    = 0;
    ctx->dst_buffer = dst_buffer;
    ctx->dst_len    = dst_size;
    ctx->dst_pos    = 0;
    ctx->error      = 0;

    decoder = FLAC__stream_decoder_new();

    if(!decoder)
    {
        free(ctx);
        return -1;
    }

    FLAC__stream_decoder_set_md5_checking(decoder, false);

    init_status = FLAC__stream_decoder_init_stream(decoder, read_callback, // 1
                                                   NULL,                   // 2 seek
                                                   NULL,                   // 3 tell
                                                   NULL,                   // 4 length
                                                   NULL,                   // 5 eof
                                                   write_callback,         // 6 write
                                                   NULL,                   // 7 metadata
                                                   error_callback,         // 8 error
                                                   ctx                     // 9 client_data
    );

    if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    {
        free(ctx);
        return -1;
    }

    FLAC__stream_decoder_process_until_end_of_stream(decoder);

    FLAC__stream_decoder_delete(decoder);

    ret_size = ctx->error ? (size_t)-1 : ctx->dst_pos;

    free(ctx);

    return ret_size;
}

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder,
                                                   FLAC__byte                 buffer[],
                                                   size_t *                   bytes,
                                                   void *                     client_data)
{
    aaru_flac_ctx *ctx       = (aaru_flac_ctx *)client_data;
    size_t         requested = *bytes;
    size_t         copied    = 0;
    size_t         chunk;

    // Synthesized header, if any, goes before the source data
    if(ctx->hdr_pos < ctx->hdr_len)
    {
        chunk = ctx->hdr_len - ctx->hdr_pos;

        if(chunk > requested) chunk = requested;

        memcpy(buffer, ctx->hdr_buffer + ctx->hdr_pos, chunk);
        ctx->hdr_pos += chunk;
        copied += chunk;
    }

    chunk = ctx->src_len - ctx->src_pos;

    if(chunk > requested - copied) chunk = requested - copied;

    memcpy(buffer + copied, ctx->src_buffer + ctx->src_pos, chunk);
    ctx->src_pos += chunk;
    copied += chunk;

    *bytes = copied;

    if(copied == 0) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder,
                                                   FLAC__uint64 *             absolute_byte_offset,
                                                   void *                     client_data)
{
    aaru_flac_ctx *ctx = (aaru_flac_ctx *)client_data;

    *absolute_byte_offset = ctx->hdr_pos + ctx->src_pos;

    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder,
                                                     const FLAC__Frame *        frame,
                                                     const FLAC__int32 *const   buffer[],
                                                     void *                     client_data)
{
    aaru_flac_ctx *ctx = (aaru_flac_ctx *)client_data;
    size_t         i;
    uint16_t *     buffer16 = (uint16_t *)(ctx->dst_buffer + ctx->dst_pos);

    // Why FLAC does not interleave the channels as PCM do, oh the mistery, we could use memcpy instead of looping
    for(i = 0; i < frame->header.blocksize && ctx->dst_pos < ctx->dst_len; i++)
    {
        // Left channel
        *(buffer16++) = (FLAC__int16)buffer[0][i];
        // Right channel
        *(buffer16++) = (FLAC__int16)buffer[1][i];

        ctx->dst_pos += 4;

        /* TODO: Big-endian (use bswap?)
        // Left channel
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)buffer[0][i];
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)buffer[0][i] >> 8;
        // Right channel
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)buffer[1][i];
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)buffer[1][i] >> 8;
        */
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    aaru_flac_ctx *ctx = (aaru_flac_ctx *)client_data;

    ctx->error = 1;
}

static FLAC__StreamEncoderWriteStatus encoder_write_callback(const FLAC__StreamEncoder *encoder,
                                                             const FLAC__byte           buffer[],
                                                             size_t                     bytes,
                                                             uint32_t                   samples,
                                                             uint32_t                   current_frame,
                                                             void *                     client_data);

static size_t flac_encode_redbook(uint8_t *      dst_buffer,
                                                             size_t         dst_size,
                                                             const uint8_t *src_buffer,
                                                             size_t         src_size,
                                                             uint32_t       blocksize,
                                                             int32_t        do_mid_side_stereo,
                                                             int32_t        loose_mid_side_stereo,
                                                             const char *   apodization,
                                                             uint32_t       max_lpc_order,
                                                             uint32_t       qlp_coeff_precision,
                                                             int32_t        do_qlp_coeff_prec_search,
                                                             int32_t        do_exhaustive_model_search,
                                                             uint32_t       min_residual_partition_order,
                                                             uint32_t       max_residual_partition_order,
                                                             const char *   application_id,
                                                             uint32_t       application_id_len)
{
    FLAC__StreamEncoder *         encoder;
    aaru_flac_ctx *               ctx = (aaru_flac_ctx *)malloc(sizeof(aaru_flac_ctx));
    FLAC__StreamEncoderInitStatus init_status;
    size_t                        ret_size;
    FLAC__int32 *                 pcm;
    int                           i;
    int16_t *                     buffer16 = (int16_t *)src_buffer;
    FLAC__StreamMetadata *        metadata[1];

    memset(ctx, 0, sizeof(aaru_flac_ctx));

    ctx->src_buffer = src_buffer;
    ctx->src_len    = src_size;
    ctx->src_pos    = 0;
    ctx->dst_buffer = dst_buffer;
    ctx->dst_len    = dst_size;
    ctx->dst_pos    = 0;
    ctx->error      = 0;

    encoder = FLAC__stream_encoder_new();

    if(!encoder)
    {
        free(ctx);
        return -1;
    }

    // TODO: Error detection here
    FLAC__stream_encoder_set_verify(encoder, false);
    FLAC__stream_encoder_set_streamable_subset(encoder, false);
    FLAC__stream_encoder_set_channels(encoder, 2);
    FLAC__stream_encoder_set_bits_per_sample(encoder, 16);
    FLAC__stream_encoder_set_sample_rate(encoder, 44100);
    FLAC__stream_encoder_set_blocksize(encoder, blocksize);
    // true compresses more
    FLAC__stream_encoder_set_do_mid_side_stereo(encoder, do_mid_side_stereo);
    // false compresses more
    FLAC__stream_encoder_set_loose_mid_side_stereo(encoder, loose_mid_side_stereo);
    // Apodization
    FLAC__stream_encoder_set_apodization(encoder, apodization);
    FLAC__stream_encoder_set_max_lpc_order(encoder, max_lpc_order);
    FLAC__stream_encoder_set_qlp_coeff_precision(encoder, qlp_coeff_precision);
    FLAC__stream_encoder_set_do_qlp_coeff_prec_search(encoder, do_qlp_coeff_prec_search);
    FLAC__stream_encoder_set_do_exhaustive_model_search(encoder, do_exhaustive_model_search);
    FLAC__stream_encoder_set_min_residual_partition_order(encoder, min_residual_partition_order);
    FLAC__stream_encoder_set_max_residual_partition_order(encoder, max_residual_partition_order);
    FLAC__stream_encoder_set_total_samples_estimate(encoder, src_size / 4);

    /* TODO: This is ignored by FLAC, need to replace it
    if((metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) != NULL)
    {
        memset(&vorbis_entry, 0, sizeof(FLAC__StreamMetadata_VorbisComment_Entry));
        vorbis_entry.entry = (unsigned char *)"Aaru.Compression.Native";
        vorbis_entry.length = strlen("Aaru.Compression.Native");

        FLAC__metadata_object_vorbiscomment_set_vendor_string(metadata[0], vorbis_entry, true);
    }
    */

    if(application_id_len > 0 && application_id != NULL)
        if((metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_APPLICATION)) != NULL)
            FLAC__metadata_object_application_set_data(metadata[0], (unsigned char *)application_id, application_id_len,
                                                       true);

    FLAC__stream_encoder_set_metadata(encoder, metadata, 1);

    init_status = FLAC__stream_encoder_init_stream(encoder, encoder_write_callback, NULL, NULL, NULL, ctx);

    if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
    {
        free(ctx);
        return -1;
    }

    pcm = malloc((src_size / 2) * sizeof(FLAC__int32));

    for(i = 0; i < src_size / 2; i++) pcm[i] = (FLAC__int32)*(buffer16++);

    FLAC__stream_encoder_process_interleaved(encoder, pcm, src_size / 4);

    FLAC__stream_encoder_finish(encoder);

    FLAC__stream_encoder_delete(encoder);

    ret_size = ctx->dst_pos;

    free(ctx);
    free(pcm);
    FLAC__metadata_object_delete(metadata[0]);

    return ret_size;
}

static FLAC__StreamEncoderWriteStatus encoder_write_callback(const FLAC__StreamEncoder *encoder,
                                                             const FLAC__byte           buffer[],
                                                             size_t                     bytes,
                                                             uint32_t                   samples,
                                                             uint32_t                   current_frame,
                                                             void *                     client_data)
{
    aaru_flac_ctx *ctx = (aaru_flac_ctx *)client_data;

    if(bytes > ctx->dst_len - ctx->dst_pos) bytes = ctx->dst_len - ctx->dst_pos;

    memcpy(ctx->dst_buffer + ctx->dst_pos, buffer, bytes);

    ctx->dst_pos += bytes;

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

AARU_EXPORT int32_t AARU_CALL AARU_flac_decode_redbook_buffer(const uint8_t *src_buffer, size_t src_size,
                                                              uint8_t *dst_buffer, size_t *dst_size)
{
    size_t written;

    if(!src_buffer || !dst_buffer || !dst_size) return AARU_ERROR_INVALID_ARGUMENT;

    written = flac_decode_redbook(dst_buffer, *dst_size, src_buffer, src_size);

    if(written == (size_t)-1) return AARU_ERROR_FAILURE;

    *dst_size = written;
    return AARU_ERROR_NONE;
}

// Decodes a stream of FLAC frames that has no "fLaC" marker nor STREAMINFO, as stored by MAME's CHD. A STREAMINFO for
// 44100Hz, 2 channels, 16-bit samples and the given block size is synthesized in front of the frames. Decoding stops
// as soon as the destination buffer is full, so any data following the frames is left untouched, and the number of
// source bytes used by the frames is returned in src_consumed.
AARU_EXPORT int32_t AARU_CALL AARU_flac_decode_redbook_frames(const uint8_t *src_buffer, size_t src_size,
                                                              uint8_t *dst_buffer, size_t *dst_size,
                                                              uint32_t block_size, size_t *src_consumed)
{
    static const uint8_t header_template[FLAC_SYNTHETIC_HEADER_LEN] = {
        0x66, 0x4C, 0x61, 0x43,                         // "fLaC"
        0x80,                                           // STREAMINFO, last metadata block
        0x00, 0x00, 0x22,                               // Metadata block length
        0x00, 0x00,                                     // Minimum block size
        0x00, 0x00,                                     // Maximum block size
        0x00, 0x00, 0x00,                               // Minimum frame size (unknown)
        0x00, 0x00, 0x00,                               // Maximum frame size (unknown)
        0x0A, 0xC4, 0x42, 0xF0, 0x00, 0x00, 0x00, 0x00, // 44100Hz, 2 channels, 16 bits, unknown samples
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MD5 (none)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t                       header[FLAC_SYNTHETIC_HEADER_LEN];
    aaru_flac_ctx                 ctx;
    FLAC__StreamDecoder          *decoder;
    FLAC__StreamDecoderInitStatus init_status;
    FLAC__uint64                  position = 0;
    int32_t                       ret      = AARU_ERROR_NONE;

    if(!src_buffer || !dst_buffer || !dst_size || !src_consumed) return AARU_ERROR_INVALID_ARGUMENT;

    if(block_size < 16 || block_size > 65535) return AARU_ERROR_INVALID_ARGUMENT;

    memcpy(header, header_template, sizeof(header));
    header[0x08] = header[0x0A] = (uint8_t)(block_size >> 8);
    header[0x09] = header[0x0B] = (uint8_t)(block_size & 0xFF);

    memset(&ctx, 0, sizeof(ctx));
    ctx.hdr_buffer = header;
    ctx.hdr_len    = sizeof(header);
    ctx.src_buffer = src_buffer;
    ctx.src_len    = src_size;
    ctx.dst_buffer = dst_buffer;
    ctx.dst_len    = *dst_size;

    decoder = FLAC__stream_decoder_new();

    if(!decoder) return AARU_ERROR_FAILURE;

    FLAC__stream_decoder_set_md5_checking(decoder, false);

    init_status = FLAC__stream_decoder_init_stream(decoder, read_callback, NULL, tell_callback, NULL, NULL,
                                                   write_callback, NULL, error_callback, &ctx);

    if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    {
        FLAC__stream_decoder_delete(decoder);
        return AARU_ERROR_FAILURE;
    }

    while(ctx.dst_pos < ctx.dst_len)
    {
        if(!FLAC__stream_decoder_process_single(decoder) || ctx.error ||
           FLAC__stream_decoder_get_state(decoder) == FLAC__STREAM_DECODER_END_OF_STREAM)
            break;
    }

    if(ctx.error || ctx.dst_pos < ctx.dst_len ||
       !FLAC__stream_decoder_get_decode_position(decoder, &position) || position < sizeof(header))
        ret = AARU_ERROR_FAILURE;
    else
    {
        *src_consumed = (size_t)(position - sizeof(header));
        *dst_size     = ctx.dst_pos;
    }

    FLAC__stream_decoder_finish(decoder);
    FLAC__stream_decoder_delete(decoder);

    return ret;
}

AARU_EXPORT int32_t AARU_CALL AARU_flac_encode_redbook_buffer(
    const uint8_t *src_buffer, size_t src_size, uint8_t *dst_buffer, size_t *dst_size, uint32_t blocksize,
    int32_t do_mid_side_stereo, int32_t loose_mid_side_stereo, const char *apodization, uint32_t max_lpc_order,
    uint32_t qlp_coeff_precision, int32_t do_qlp_coeff_prec_search, int32_t do_exhaustive_model_search,
    uint32_t min_residual_partition_order, uint32_t max_residual_partition_order, const char *application_id,
    uint32_t application_id_len)
{
    size_t written;

    if(!src_buffer || !dst_buffer || !dst_size) return AARU_ERROR_INVALID_ARGUMENT;

    written = flac_encode_redbook(dst_buffer, *dst_size, src_buffer, src_size, blocksize, do_mid_side_stereo,
                                  loose_mid_side_stereo, apodization, max_lpc_order, qlp_coeff_precision,
                                  do_qlp_coeff_prec_search, do_exhaustive_model_search, min_residual_partition_order,
                                  max_residual_partition_order, application_id, application_id_len);

    if(written == (size_t)-1) return AARU_ERROR_FAILURE;

    *dst_size = written;
    return AARU_ERROR_NONE;
}
