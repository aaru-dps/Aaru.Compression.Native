/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2021 Natalia Portillo.
 * Copyright © 2018-2019 David Ryskalczyk
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

#include <stdint.h>

#include "library.h"
#include "3rdparty/lzlib-1.12/lzlib.h"
#include "lzip.h"

AARU_EXPORT int32_t AARU_CALL lzip_decode_buffer(uint8_t*       dst_buffer,
                                                 int32_t        dst_size,
                                                 const uint8_t* src_buffer,
                                                 int32_t        src_size)
{
    int           max_in_size;
    void*         ctx;
    enum LZ_Errno lz_err;
    int32_t       in_pos = 0, out_pos = 0, in_size;
    int           rd;
    // Initialize the decoder
    ctx    = LZ_decompress_open();
    lz_err = LZ_decompress_errno(ctx);

    if(lz_err != LZ_ok)
    {
        // Ensure freed
        LZ_decompress_close(ctx);
        return 0;
    }

    // Check maximum lzip buffer input size
    max_in_size = LZ_decompress_write_size(ctx);
    if(src_size < max_in_size) max_in_size = src_size;

    // Decompress buffer
    while(in_pos < src_size && out_pos < dst_size)
    {
        if(src_size - in_pos < max_in_size) max_in_size = src_size - in_pos;

        in_size = LZ_decompress_write(ctx, src_buffer + in_pos, max_in_size);

        in_pos += in_size;

        rd = LZ_decompress_read(ctx, dst_buffer + out_pos, dst_size);

        out_pos += rd;

        if(LZ_decompress_finished(ctx) == 1) break;
    }

    LZ_decompress_finish(ctx);

    if(out_pos < dst_size)
    {
        do {
            rd = LZ_decompress_read(ctx, dst_buffer + out_pos, dst_size);

            out_pos += rd;

            if(LZ_compress_finished(ctx) == 1) break;
        } while(rd > 0 && out_pos < dst_size);
    }

    // Free buffers
    LZ_decompress_close(ctx);

    return out_pos;
}

AARU_EXPORT int32_t AARU_CALL lzip_encode_buffer(uint8_t*       dst_buffer,
                                                 int32_t        dst_size,
                                                 const uint8_t* src_buffer,
                                                 int32_t        src_size,
                                                 int32_t        dictionary_size,
                                                 int32_t        match_len_limit)
{
    int           max_in_size;
    void*         ctx;
    enum LZ_Errno lz_err;
    int32_t       in_pos = 0, out_pos = 0, in_size;
    int           rd;

    // Initialize the decoder
    ctx    = LZ_compress_open(dictionary_size, match_len_limit, src_size);
    lz_err = LZ_compress_errno(ctx);

    if(lz_err != LZ_ok)
    {
        // Ensure freed
        LZ_compress_close(ctx);
        return 0;
    }

    // Check maximum lzip buffer input size
    max_in_size = LZ_compress_write_size(ctx);
    if(src_size < max_in_size) max_in_size = src_size;

    // Compress buffer
    while(in_pos < src_size && out_pos < dst_size)
    {
        if(src_size - in_pos < max_in_size) max_in_size = src_size - in_pos;

        in_size = LZ_compress_write(ctx, src_buffer + in_pos, max_in_size);

        in_pos += in_size;

        rd = LZ_compress_read(ctx, dst_buffer + out_pos, dst_size);

        out_pos += rd;

        if(LZ_compress_finished(ctx) == 1) break;
    }

    LZ_compress_finish(ctx);

    if(out_pos < dst_size)
    {
        do {
            rd = LZ_compress_read(ctx, dst_buffer + out_pos, dst_size);

            out_pos += rd;

            if(LZ_compress_finished(ctx) == 1) break;
        } while(rd > 0 && out_pos < dst_size);
    }

    // Free buffers
    LZ_compress_close(ctx);

    return out_pos;
}