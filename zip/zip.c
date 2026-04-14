/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2026 Natalia Portillo.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "zip.h"

#include <stdlib.h>
#include <string.h>

/* PPMd includes */
#include "../ppmd/SubAllocatorVariantI.h"
#include "../ppmd/VariantI.h"

/* WavPack includes */
#include "../wavpack/wavpack.h"

/* WinZipJPEG includes */
#include "../winzipjpeg/Decompressor.h"

/* ============== PPMd Wrapper ============== */

typedef struct
{
    const uint8_t *data;
    size_t         size;
    size_t         pos;
} PPMdBufferContext;

static int ppmd_read_byte(void *context)
{
    PPMdBufferContext *ctx = (PPMdBufferContext *)context;

    if(ctx->pos >= ctx->size) return -1;

    return ctx->data[ctx->pos++];
}

int zip_ppmd_decompress(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size, int max_order,
                        int sub_alloc_size, int restoration)
{
    PPMdSubAllocatorVariantI *alloc;
    PPMdModelVariantI         model;
    PPMdBufferContext         ctx;

    if(!dst_buffer || !src_buffer) return -1;

    ctx.data = src_buffer;
    ctx.size = src_size;
    ctx.pos  = 0;

    alloc = CreateSubAllocatorVariantI(sub_alloc_size);

    if(!alloc) return -1;

    StartPPMdModelVariantI(&model, ppmd_read_byte, &ctx, alloc, max_order, restoration);

    for(size_t i = 0; i < dst_size; i++)
    {
        int byte = NextPPMdVariantIByte(&model);

        if(byte < 0)
        {
            FreeSubAllocatorVariantI(alloc);
            return -1;
        }

        dst_buffer[i] = (uint8_t)byte;
    }

    FreeSubAllocatorVariantI(alloc);
    return 0;
}

/* ============== WavPack Wrapper ============== */

typedef struct
{
    const uint8_t *data;
    size_t         size;
    size_t         pos;
    int            pushback;
    int            has_pushback;
} WavpackBufferContext;

static int32_t wavpack_read_bytes(void *id, void *data, int32_t bcount)
{
    WavpackBufferContext *ctx       = (WavpackBufferContext *)id;
    int32_t               available = (int32_t)(ctx->size - ctx->pos);
    int32_t               to_read   = bcount < available ? bcount : available;

    if(ctx->has_pushback && to_read > 0)
    {
        ((uint8_t *)data)[0] = (uint8_t)ctx->pushback;
        ctx->has_pushback    = 0;
        memcpy((uint8_t *)data + 1, ctx->data + ctx->pos, to_read - 1);
        ctx->pos += to_read - 1;
    }
    else
    {
        memcpy(data, ctx->data + ctx->pos, to_read);
        ctx->pos += to_read;
    }

    return to_read;
}

static uint32_t wavpack_get_pos(void *id)
{
    WavpackBufferContext *ctx = (WavpackBufferContext *)id;
    return (uint32_t)ctx->pos;
}

static int wavpack_set_pos_abs(void *id, uint32_t pos)
{
    WavpackBufferContext *ctx = (WavpackBufferContext *)id;

    if(pos > ctx->size) return -1;

    ctx->pos          = pos;
    ctx->has_pushback = 0;
    return 0;
}

static int wavpack_set_pos_rel(void *id, int32_t delta, int mode)
{
    WavpackBufferContext *ctx = (WavpackBufferContext *)id;
    int64_t               newpos;

    switch(mode)
    {
        case 0: /* SEEK_SET */
            newpos = delta;
            break;
        case 1: /* SEEK_CUR */
            newpos = (int64_t)ctx->pos + delta;
            break;
        case 2: /* SEEK_END */
            newpos = (int64_t)ctx->size + delta;
            break;
        default:
            return -1;
    }

    if(newpos < 0 || (size_t)newpos > ctx->size) return -1;

    ctx->pos          = (size_t)newpos;
    ctx->has_pushback = 0;
    return 0;
}

static int wavpack_push_back_byte(void *id, int c)
{
    WavpackBufferContext *ctx = (WavpackBufferContext *)id;
    ctx->pushback             = c;
    ctx->has_pushback         = 1;
    return c;
}

static uint32_t wavpack_get_length(void *id)
{
    WavpackBufferContext *ctx = (WavpackBufferContext *)id;
    return (uint32_t)ctx->size;
}

static int wavpack_can_seek(void *id)
{
    (void)id;
    return 1;
}

int zip_wavpack_decompress(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size,
                           uint32_t num_samples, int bits_per_sample, int num_channels)
{
    WavpackBufferContext ctx;
    WavpackStreamReader  reader;
    WavpackContext      *wpc;
    char                 error[80];
    int                  bytes_per_sample;
    int32_t             *sample_buf;
    uint32_t             decoded;
    size_t               out_pos;

    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    ctx.data         = src_buffer;
    ctx.size         = src_size;
    ctx.pos          = 0;
    ctx.pushback     = 0;
    ctx.has_pushback = 0;

    reader.read_bytes     = wavpack_read_bytes;
    reader.get_pos        = wavpack_get_pos;
    reader.set_pos_abs    = wavpack_set_pos_abs;
    reader.set_pos_rel    = wavpack_set_pos_rel;
    reader.push_back_byte = wavpack_push_back_byte;
    reader.get_length     = wavpack_get_length;
    reader.can_seek       = wavpack_can_seek;
    reader.write_bytes    = NULL;

    wpc = WavpackOpenFileInputEx(&reader, &ctx, NULL, error, 0, 0);

    if(!wpc) return -1;

    bytes_per_sample = (bits_per_sample + 7) / 8;

    /* Allocate sample buffer for one block of samples */
    sample_buf = (int32_t *)malloc(num_samples * num_channels * sizeof(int32_t));

    if(!sample_buf)
    {
        WavpackCloseFile(wpc);
        return -1;
    }

    decoded = WavpackUnpackSamples(wpc, sample_buf, num_samples);
    out_pos = 0;

    /* Compact int32 samples to actual byte width */
    for(uint32_t i = 0; i < decoded * (uint32_t)num_channels; i++)
    {
        int32_t sample = sample_buf[i];

        for(int b = 0; b < bytes_per_sample; b++)
        {
            if(out_pos < *dst_size) dst_buffer[out_pos++] = (uint8_t)(sample & 0xFF);

            sample >>= 8;
        }
    }

    *dst_size = out_pos;

    free(sample_buf);
    WavpackCloseFile(wpc);
    return 0;
}

/* ============== WinZipJPEG Wrapper ============== */

typedef struct
{
    const uint8_t *data;
    size_t         size;
    size_t         pos;
} WinZipJPEGBufferContext;

static size_t winzipjpeg_read(void *context, uint8_t *buffer, size_t length)
{
    WinZipJPEGBufferContext *ctx       = (WinZipJPEGBufferContext *)context;
    size_t                   remaining = ctx->size - ctx->pos;
    size_t                   to_read   = length < remaining ? length : remaining;

    memcpy(buffer, ctx->data + ctx->pos, to_read);
    ctx->pos += to_read;

    return to_read;
}

int zip_winzipjpeg_decompress(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size)
{
    WinZipJPEGBufferContext ctx;
    WinZipJPEGDecompressor *decompressor;
    size_t                  out_pos  = 0;
    size_t                  out_size = *dst_size;
    int                     err;

    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    ctx.data = src_buffer;
    ctx.size = src_size;
    ctx.pos  = 0;

    decompressor = AllocWinZipJPEGDecompressor(winzipjpeg_read, &ctx);

    if(!decompressor) return -1;

    err = ReadWinZipJPEGHeader(decompressor);

    if(err != WinZipJPEGNoError)
    {
        FreeWinZipJPEGDecompressor(decompressor);
        return err;
    }

    /* Process bundles */
    while(!IsFinalWinZipJPEGBundle(decompressor))
    {
        err = ReadNextWinZipJPEGBundle(decompressor);

        if(err != WinZipJPEGNoError) break;

        /* Copy metadata bytes */
        uint32_t meta_len = WinZipJPEGBundleMetadataLength(decompressor);
        uint8_t *meta     = WinZipJPEGBundleMetadataBytes(decompressor);

        for(uint32_t i = 0; i < meta_len && out_pos < out_size; i++) dst_buffer[out_pos++] = meta[i];

        /* Process slices */
        while(AreMoreWinZipJPEGSlicesAvailable(decompressor))
        {
            err = ReadNextWinZipJPEGSlice(decompressor);

            if(err != WinZipJPEGNoError) break;

            /* Encode blocks to output buffer */
            while(AreMoreWinZipJPEGBytesAvailable(decompressor) && out_pos < out_size)
            {
                size_t written = EncodeWinZipJPEGBlocksToBuffer(decompressor, dst_buffer + out_pos, out_size - out_pos);
                out_pos += written;

                if(written == 0) break;
            }
        }

        if(err != WinZipJPEGNoError) break;
    }

    *dst_size = out_pos;
    FreeWinZipJPEGDecompressor(decompressor);
    return (err == WinZipJPEGNoError) ? 0 : err;
}
