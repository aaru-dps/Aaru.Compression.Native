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

static void ace_v1_decompress_core(ace_decompress_ctx_t *ctx)
{
    int      c, lg, i, k;
    uint32_t dist;

    while(ctx->block_byte_count < ctx->block_buf_size && !ctx->error)
    {
        if(!ctx->block_size)
        {
            if(!ace_read_widths(ctx, ACE_MAX_CODE_WIDTH, ctx->main_huff_symbols, ctx->main_huff_widths,
                                ACE_MAX_MAIN_CODE) ||
               !ace_read_widths(ctx, ACE_MAX_CODE_WIDTH, ctx->len_huff_symbols, ctx->len_huff_widths, ACE_MAX_LEN_CODE))
            {
                ctx->error = 1;
                return;
            }
            ctx->block_size = ctx->read_code >> (32 - 15);
            ace_add_bits(ctx, 15);
        }

        c = ctx->main_huff_symbols[ctx->read_code >> (32 - ACE_MAX_CODE_WIDTH)];
        ace_add_bits(ctx, ctx->main_huff_widths[c]);
        ctx->block_size--;

        if(c > 255)
        {
            if(c > 259)
            {
                if((c -= 260) > 1)
                {
                    dist = (ctx->read_code >> (33 - c)) + (1u << (c - 1));
                    ace_add_bits(ctx, c - 1);
                }
                else
                {
                    dist = (uint32_t)c;
                }
                ctx->old_dists[(ctx->old_dists_pos = (ctx->old_dists_pos + 1) & 3)] = dist;
                i                                                                   = 2;
                if(dist > ACE_MAX_DIST_AT_LEN2)
                {
                    i++;
                    if(dist > ACE_MAX_DIST_AT_LEN3) i++;
                }
            }
            else
            {
                dist = ctx->old_dists[(ctx->old_dists_pos - (c &= 255)) & 3];
                for(k = c + 1; k--;)
                    ctx->old_dists[(ctx->old_dists_pos - k) & 3] = ctx->old_dists[(ctx->old_dists_pos - k + 1) & 3];
                ctx->old_dists[ctx->old_dists_pos] = dist;
                i                                  = 2;
                if(c > 1) i++;
            }

            lg = ctx->len_huff_symbols[ctx->read_code >> (32 - ACE_MAX_CODE_WIDTH)];
            ace_add_bits(ctx, ctx->len_huff_widths[lg]);
            lg += i;
            dist++;
            ace_lz77_copy_string(ctx, dist, lg);
        }
        else
        {
            ace_lz77_write_char(ctx, (uint8_t)c);
        }
    }
}

int ace_decompress_v1(ace_decompress_ctx_t *ctx, const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                      size_t *out_len)
{
    size_t   orig_size = *out_len;
    uint32_t old_pos;
    int      i;

    ctx->in_buf  = in_buf;
    ctx->in_len  = in_len;
    ctx->in_pos  = 0;
    ctx->error   = 0;
    ctx->dic_pos = 0;

    ctx->file_size        = (int64_t)orig_size;
    ctx->block_size       = 0;
    ctx->block_byte_count = 0;
    ctx->old_dists_pos    = 0;
    memset(ctx->old_dists, 0, sizeof(ctx->old_dists));

    ace_init_read_buf(ctx);

    {
        size_t total_out = 0;

        while(ctx->file_size > 0 && !ctx->error)
        {
            old_pos               = ctx->dic_pos;
            ctx->block_byte_count = 0;
            ctx->block_buf_size   = (uint32_t)ctx->file_size;

            if(ctx->block_buf_size > ctx->dic_size - ACE_MAX_LEN) ctx->block_buf_size = ctx->dic_size - ACE_MAX_LEN;

            ace_v1_decompress_core(ctx);

            if(ctx->block_byte_count > 0 && total_out + ctx->block_byte_count <= orig_size)
            {
                if(old_pos + ctx->block_byte_count > ctx->dic_size)
                {
                    i = ctx->dic_size - old_pos;
                    memcpy(out_buf + total_out, &ctx->dictionary[old_pos], i);
                    memcpy(out_buf + total_out + i, ctx->dictionary, ctx->block_byte_count - i);
                }
                else
                {
                    memcpy(out_buf + total_out, &ctx->dictionary[old_pos], ctx->block_byte_count);
                }
            }

            total_out += ctx->block_byte_count;
            ctx->file_size -= ctx->block_byte_count;
        }

        *out_len = total_out;
    }

    return ctx->error ? -1 : 0;
}
