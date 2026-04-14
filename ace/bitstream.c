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

#include <string.h>

#include "ace.h"
#include "ace_internal.h"

void ace_fill_read_buf(ace_decompress_ctx_t *ctx)
{
    uint32_t i;
    size_t   bytes_to_read;
    size_t   words_to_read;

    ctx->read_buf[0] = ctx->read_buf[ACE_READ_BUF_SIZE - 2];
    ctx->read_buf[1] = ctx->read_buf[ACE_READ_BUF_SIZE - 1];
    ctx->read_buf_pos -= ACE_READ_BUF_SIZE - 2;

    bytes_to_read = (ACE_READ_BUF_SIZE - 2) * 4;
    if(bytes_to_read > ctx->in_len - ctx->in_pos) bytes_to_read = ctx->in_len - ctx->in_pos;

    words_to_read = bytes_to_read / 4;

    for(i = 0; i < words_to_read; i++)
    {
        size_t off           = ctx->in_pos + i * 4;
        ctx->read_buf[2 + i] = (uint32_t)ctx->in_buf[off] | ((uint32_t)ctx->in_buf[off + 1] << 8) |
                               ((uint32_t)ctx->in_buf[off + 2] << 16) | ((uint32_t)ctx->in_buf[off + 3] << 24);
    }

    if(bytes_to_read > words_to_read * 4)
    {
        uint32_t word = 0;
        size_t   off  = ctx->in_pos + words_to_read * 4;
        size_t   rem  = bytes_to_read - words_to_read * 4;
        size_t   b;
        for(b = 0; b < rem; b++) word |= (uint32_t)ctx->in_buf[off + b] << (b * 8);
        ctx->read_buf[2 + words_to_read] = word;
    }

    ctx->in_pos += bytes_to_read;
}

void ace_add_bits(ace_decompress_ctx_t *ctx, int bits)
{
    uint32_t b0, b1;

    ctx->read_buf_pos += (ctx->read_code_bit_pos += bits) >> 5;
    ctx->read_code_bit_pos &= 31;

    if(ctx->read_buf_pos == ACE_READ_BUF_SIZE - 2) ace_fill_read_buf(ctx);

    b0 = ctx->read_buf[ctx->read_buf_pos];
    b1 = ctx->read_buf[ctx->read_buf_pos + 1];

    ctx->read_code = (b0 << ctx->read_code_bit_pos) +
                     ((b1 >> (32 - ctx->read_code_bit_pos)) & ((uint32_t)(!ctx->read_code_bit_pos) - 1));
}

void ace_init_read_buf(ace_decompress_ctx_t *ctx)
{
    size_t bytes_to_read;
    size_t words_to_read;
    size_t i;

    bytes_to_read = ACE_READ_BUF_SIZE * 4;
    if(bytes_to_read > ctx->in_len - ctx->in_pos) bytes_to_read = ctx->in_len - ctx->in_pos;

    words_to_read = bytes_to_read / 4;

    for(i = 0; i < words_to_read; i++)
    {
        size_t off       = ctx->in_pos + i * 4;
        ctx->read_buf[i] = (uint32_t)ctx->in_buf[off] | ((uint32_t)ctx->in_buf[off + 1] << 8) |
                           ((uint32_t)ctx->in_buf[off + 2] << 16) | ((uint32_t)ctx->in_buf[off + 3] << 24);
    }

    if(bytes_to_read > words_to_read * 4)
    {
        uint32_t word = 0;
        size_t   off  = ctx->in_pos + words_to_read * 4;
        size_t   rem  = bytes_to_read - words_to_read * 4;
        size_t   b;
        for(b = 0; b < rem; b++) word |= (uint32_t)ctx->in_buf[off + b] << (b * 8);
        ctx->read_buf[words_to_read] = word;
    }

    ctx->in_pos += bytes_to_read;

    ctx->read_code         = ctx->read_buf[0];
    ctx->read_code_bit_pos = 0;
    ctx->read_buf_pos      = 0;
}
