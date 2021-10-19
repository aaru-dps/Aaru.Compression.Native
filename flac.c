//
// Created by claunia on 20/10/21.
//

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "library.h"
#include "FLAC/stream_decoder.h"
#include "flac.h"

static FLAC__StreamDecoderReadStatus
    read_callback(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder* decoder,
                                                     const FLAC__Frame*         frame,
                                                     const FLAC__int32* const   buffer[],
                                                     void*                      client_data);
static void
    error_callback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data);

AARU_EXPORT size_t AARU_CALL flac_decode_redbook_buffer(uint8_t*       dst_buffer,
                                                        size_t         dst_size,
                                                        const uint8_t* src_buffer,
                                                        size_t         src_size)
{
    FLAC__StreamDecoder*          decoder;
    FLAC__StreamDecoderInitStatus init_status;
    aaru_flac_ctx*                ctx = (aaru_flac_ctx*)malloc(sizeof(aaru_flac_ctx));
    FLAC__bool                    ok  = true;
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

    init_status = FLAC__stream_decoder_init_stream(
        decoder, read_callback, NULL, NULL, NULL, NULL, write_callback, NULL, error_callback, ctx);

    if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    {
        free(ctx);
        return -1;
    }

    // TODO: Return error somehow
    ok = FLAC__stream_decoder_process_until_end_of_stream(decoder);

    FLAC__stream_decoder_delete(decoder);

    ret_size = ctx->dst_pos;

    free(ctx);

    return ret_size;
}

static FLAC__StreamDecoderReadStatus
    read_callback(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data)
{
    aaru_flac_ctx* ctx = (aaru_flac_ctx*)client_data;

    if(ctx->src_len - ctx->src_pos < *bytes) *bytes = ctx->src_len - ctx->src_pos;

    if(*bytes == 0) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

    memcpy(buffer, ctx->src_buffer + ctx->src_pos, *bytes);
    ctx->src_pos += *bytes;

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder* decoder,
                                                     const FLAC__Frame*         frame,
                                                     const FLAC__int32* const   bufffer[],
                                                     void*                      client_data)
{
    aaru_flac_ctx* ctx = (aaru_flac_ctx*)client_data;
    size_t         i;

    for(i = 0; i < frame->header.blocksize && ctx->dst_pos < ctx->dst_len; i++)
    {
        // Left channel
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)bufffer[0][i];
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)bufffer[0][i] >> 8;
        // Right channel
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)bufffer[1][i];
        ctx->dst_buffer[ctx->dst_pos++] = (FLAC__uint16)(FLAC__int16)bufffer[1][i] >> 8;
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void error_callback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data)
{
    aaru_flac_ctx* ctx = (aaru_flac_ctx*)client_data;

    fprintf(stderr, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);

    ctx->error = 1;
}
