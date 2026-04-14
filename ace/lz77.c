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

void ace_lz77_write_char(ace_decompress_ctx_t *ctx, uint8_t ch)
{
    ctx->dictionary[ctx->dic_pos] = ch;
    ctx->dic_pos                  = (ctx->dic_pos + 1) & ctx->dic_and;
    ctx->block_byte_count++;
}

void ace_lz77_copy_string(ace_decompress_ctx_t *ctx, uint32_t dist, int len)
{
    uint32_t src_pos;
    int      i;

    src_pos = (ctx->dic_pos - dist) & ctx->dic_and;

    if(src_pos >= ctx->dic_size - ACE_MAX_LEN || ctx->dic_pos >= ctx->dic_size - ACE_MAX_LEN)
    {
        for(i = len; i--;)
        {
            ctx->dictionary[ctx->dic_pos] = ctx->dictionary[src_pos];
            ctx->dic_pos                  = (ctx->dic_pos + 1) & ctx->dic_and;
            src_pos                       = (src_pos + 1) & ctx->dic_and;
        }
    }
    else
    {
        for(i = 0; i < len; i++) ctx->dictionary[ctx->dic_pos + i] = ctx->dictionary[src_pos + i];
        ctx->dic_pos = (ctx->dic_pos + len) & ctx->dic_and;
    }

    ctx->block_byte_count += len;
}

int ace_lz77_calc_huff_tabs(ace_decompress_ctx_t *ctx)
{
    if(!ace_read_widths(ctx, ACE_MAX_CODE_WIDTH, ctx->main_huff_symbols, ctx->main_huff_widths, ACE_MAX_MAIN_CODE) ||
       !ace_read_widths(ctx, ACE_MAX_CODE_WIDTH, ctx->len_huff_symbols, ctx->len_huff_widths, ACE_MAX_LEN_CODE))
        return 0;

    ctx->block_size = ctx->read_code >> (32 - 15);
    ace_add_bits(ctx, 15);

    return 1;
}

int ace_lz77_read_symbols(ace_decompress_ctx_t *ctx)
{
    int symbol_count, match_count, symbol, type;

    if(!ctx->block_size && !ace_lz77_calc_huff_tabs(ctx)) return 0;

    ctx->part_size = ACE_MAX_PART_SIZE > ctx->block_size ? ctx->block_size : ACE_MAX_PART_SIZE;
    ctx->block_size -= ctx->part_size;

    match_count = 0;

    for(symbol_count = 0; (uint32_t)symbol_count < ctx->part_size; symbol_count++)
    {
        symbol = ctx->main_huff_symbols[ctx->read_code >> (32 - ACE_MAX_CODE_WIDTH)];
        ace_add_bits(ctx, ctx->main_huff_widths[symbol]);
        ctx->main_buf[symbol_count] = (uint16_t)symbol;

        if(symbol > 255)
        {
            if(symbol == ACE_TYPE_CODE)
            {
                type                      = ctx->read_code >> 24;
                ctx->len_buf[match_count] = (uint16_t)type;
                ace_add_bits(ctx, 8);

                switch(type)
                {
                    case ACE_BLOCK_LZ77_DELTA:
                        ctx->dist_buf[match_count] = ctx->read_code >> (32 - 25);
                        ace_add_bits(ctx, 25);
                        break;
                    case ACE_BLOCK_LZ77_EXE:
                        ctx->dist_buf[match_count] = ctx->read_code >> (32 - 8);
                        ace_add_bits(ctx, 8);
                        break;
                    default:
                        break;
                }

                match_count++;
            }
            else
            {
                if(symbol > 259)
                {
                    if((symbol -= 260) > 1)
                    {
                        ctx->dist_buf[match_count] = (ctx->read_code >> (33 - symbol)) + (1u << (symbol - 1));
                        ace_add_bits(ctx, symbol - 1);
                    }
                    else
                    {
                        ctx->dist_buf[match_count] = (uint32_t)symbol;
                    }
                }

                ctx->len_buf[match_count] = ctx->len_huff_symbols[ctx->read_code >> (32 - ACE_MAX_CODE_WIDTH)];
                ace_add_bits(ctx, ctx->len_huff_widths[ctx->len_buf[match_count]]);
                match_count++;
            }
        }
    }

    ctx->len_dist_buf_pos = ctx->main_buf_pos = 0;
    return 1;
}

void ace_lz77_block_core(ace_decompress_ctx_t *ctx)
{
    int      symbol, len_symbol, i;
    uint32_t match_dist;

    while(ctx->block_byte_count < ctx->block_buf_size)
    {
        if(ctx->main_buf_pos == ctx->part_size)
        {
            if(!ace_lz77_read_symbols(ctx)) return;
        }

        symbol = ctx->main_buf[ctx->main_buf_pos++];

        if(symbol > 255)
        {
            if(symbol == ACE_TYPE_CODE)
            {
                ctx->next_type = ctx->len_buf[ctx->len_dist_buf_pos] & 255;

                switch(ctx->next_type)
                {
                    case ACE_BLOCK_LZ77_DELTA:
                        ctx->next_delta_dist = ctx->dist_buf[ctx->len_dist_buf_pos] >> 17;
                        ctx->next_delta_len  = ctx->dist_buf[ctx->len_dist_buf_pos] & 0x1FFFF;
                        break;
                    case ACE_BLOCK_LZ77_EXE:
                        ctx->next_exe_mode = ctx->dist_buf[ctx->len_dist_buf_pos];
                        break;
                    default:
                        break;
                }

                ctx->len_dist_buf_pos++;
                break;
            }

            if(symbol > 259)
            {
                symbol -= 260;
                match_dist = ctx->dist_buf[ctx->len_dist_buf_pos];

                ctx->old_dists_pos                 = (ctx->old_dists_pos + 1) & 3;
                ctx->old_dists[ctx->old_dists_pos] = match_dist;

                i = 2;
                if(match_dist > ACE_MAX_DIST_AT_LEN2)
                {
                    i++;
                    if(match_dist > ACE_MAX_DIST_AT_LEN3) i++;
                }
            }
            else
            {
                match_dist = ctx->old_dists[(ctx->old_dists_pos - (symbol &= 255)) & 3];

                for(i = symbol; i >= 0; i--)
                    ctx->old_dists[(ctx->old_dists_pos - i) & 3] = ctx->old_dists[(ctx->old_dists_pos - i + 1) & 3];

                ctx->old_dists[ctx->old_dists_pos] = match_dist;
                i                                  = symbol > 1 ? 3 : 2;
            }

            len_symbol = ctx->len_buf[ctx->len_dist_buf_pos++] + i;
            match_dist++;
            ace_lz77_copy_string(ctx, match_dist, len_symbol);
        }
        else
        {
            ace_lz77_write_char(ctx, (uint8_t)symbol);
        }
    }
}

int ace_lz77_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len)
{
    int old_pos, i;

    old_pos               = ctx->dic_pos;
    ctx->block_byte_count = 0;

    if(len < ACE_MAX_LEN)
    {
        if(ctx->file_size <= 0 || (int64_t)len < ctx->file_size) return 0;
        ctx->block_buf_size = (uint32_t)ctx->file_size;
    }
    else
    {
        ctx->block_buf_size = len - ACE_MAX_LEN;
    }

    if((int64_t)ctx->block_buf_size > ctx->file_size) ctx->block_buf_size = (uint32_t)ctx->file_size;
    if(ctx->block_buf_size > ctx->dic_size - ACE_MAX_LEN) ctx->block_buf_size = ctx->dic_size - ACE_MAX_LEN;

    if(ctx->file_size > 0 && ctx->block_buf_size)
    {
        ace_lz77_block_core(ctx);

        if(ctx->block_byte_count <= (uint32_t)len)
        {
            if((uint32_t)old_pos + ctx->block_byte_count > ctx->dic_size)
            {
                i = ctx->dic_size - old_pos;
                memcpy(buf, &ctx->dictionary[old_pos], i);
                memcpy(&buf[i], ctx->dictionary, ctx->block_byte_count - i);
            }
            else
            {
                memcpy(buf, &ctx->dictionary[old_pos], ctx->block_byte_count);
            }
        }
    }

    ctx->file_size -= ctx->block_byte_count;
    return (int)ctx->block_byte_count;
}

void ace_lz77_copy_to_dict(ace_decompress_ctx_t *ctx, const uint8_t *buf, int len)
{
    int i;
    for(i = 0; i < len; i++)
    {
        ctx->dictionary[ctx->dic_pos] = buf[i];
        ctx->dic_pos                  = (ctx->dic_pos + 1) & ctx->dic_and;
    }
}
