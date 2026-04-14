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

int ace_decompress_v20_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len)
{
    int r, rest_len;

    rest_len = len;

    do
    {
        if(ctx->type != ctx->next_type)
        {
            switch(ctx->next_type)
            {
                case ACE_BLOCK_SOUND_8:
                case ACE_BLOCK_SOUND_16:
                case ACE_BLOCK_SOUND_32_1:
                case ACE_BLOCK_SOUND_32_2:
                    ace_sound_init(ctx, ctx->next_type);
                    break;
                case ACE_BLOCK_PIC:
                    ace_pic_init(ctx);
                    break;
                default:
                    break;
            }
        }

        ctx->type = ctx->next_type;

        switch(ctx->type)
        {
            case ACE_BLOCK_LZ77_NORM:
            case ACE_BLOCK_LZ77_DELTA:
            case ACE_BLOCK_LZ77_EXE:
                r = ace_lz77_preprocess_block(ctx, buf, rest_len);
                break;
            case ACE_BLOCK_PIC:
                r = ace_pic_block(ctx, buf, rest_len);
                ace_lz77_copy_to_dict(ctx, buf, r);
                break;
            case ACE_BLOCK_SOUND_8:
            case ACE_BLOCK_SOUND_16:
            case ACE_BLOCK_SOUND_32_1:
            case ACE_BLOCK_SOUND_32_2:
                r = ace_sound_block(ctx, buf, rest_len);
                ace_lz77_copy_to_dict(ctx, buf, r);
                break;
            default:
                r = 0;
                break;
        }

        ctx->file_pos += r;
        buf += r;
        rest_len -= r;
    } while((r || ctx->next_type != ctx->type || (ctx->next_type == ACE_BLOCK_LZ77_DELTA && ctx->next_delta_len)) &&
            (rest_len > ACE_MAX_LEN || (ctx->file_size > 0 && ctx->file_size <= ACE_MAX_LEN && rest_len > 0)));

    return len - rest_len;
}

int ace_lz77_preprocess_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len)
{
    int i, r, pos, read_count;

    if(ctx->type == ACE_BLOCK_LZ77_DELTA)
    {
        if(!ctx->prep_num_kept_bytes)
        {
            ctx->delta_dist     = ctx->next_delta_dist;
            ctx->delta_len      = ctx->next_delta_len;
            ctx->next_delta_len = 0;

            do
            {
                ctx->prep_num_kept_bytes +=
                    (read_count = ace_lz77_block(ctx, ctx->prep_kept_bytes_buf + ctx->prep_num_kept_bytes,
                                                 ACE_MAX_DELTA_BLOCK + ACE_MAX_LEN - ctx->prep_num_kept_bytes));
            } while(ctx->prep_num_kept_bytes < ctx->delta_len);

            ctx->delta_block_size = ctx->prep_num_kept_bytes;

            if(ctx->delta_dist)
                ctx->delta_plane_size = ctx->delta_block_size / ctx->delta_dist;
            else
                ctx->delta_plane_size = 0;

            ctx->over_next_type = ctx->next_type;
            ctx->next_type      = ACE_BLOCK_LZ77_DELTA;

            for(i = 0; i < ctx->delta_block_size; i++)
                ctx->prep_last_delta = (ctx->prep_kept_bytes_buf[i] += (uint8_t)ctx->prep_last_delta);

            ctx->prep_kept_bytes_pos = ctx->delta_plane = ctx->delta_plane_pos = 0;
        }

        r = ctx->prep_num_kept_bytes > len ? len : ctx->prep_num_kept_bytes;

        if(ctx->delta_plane_size)
        {
            for(i = 0;; ctx->delta_plane_pos++)
            {
                for(; ctx->delta_plane < ctx->delta_block_size; ctx->delta_plane += ctx->delta_plane_size, i++)
                {
                    if(i == r) goto DELTA_BREAK;
                    buf[i] = ctx->prep_kept_bytes_buf[ctx->delta_plane_pos + ctx->delta_plane];
                }
                ctx->delta_plane = 0;
            }
        }

    DELTA_BREAK:
        ctx->prep_kept_bytes_pos += r;
        ctx->prep_num_kept_bytes -= r;

        if(!ctx->prep_num_kept_bytes) ctx->next_type = ctx->over_next_type;

        return r;
    }
    else
    {
        memcpy(buf, ctx->prep_kept_bytes_buf, ctx->prep_num_kept_bytes);

        r = ace_lz77_block(ctx, &buf[ctx->prep_num_kept_bytes], len - ctx->prep_num_kept_bytes) +
            ctx->prep_num_kept_bytes;

        ctx->prep_num_kept_bytes = 0;

        if(ctx->type == ACE_BLOCK_LZ77_EXE)
        {
            ctx->exe_mode = ctx->next_exe_mode;

            for(i = 0; i < r - 4; i++)
            {
                pos = (int)(ctx->file_pos + i);

                if(buf[i] == (uint8_t)0xe8)
                {
                    if(!ctx->exe_mode)
                    {
                        uint16_t val;
                        memcpy(&val, &buf[i + 1], 2);
                        val -= (uint16_t)pos;
                        memcpy(&buf[i + 1], &val, 2);
                        i += 2;
                    }
                    else
                    {
                        uint32_t val;
                        memcpy(&val, &buf[i + 1], 4);
                        val -= (uint32_t)pos;
                        memcpy(&buf[i + 1], &val, 4);
                        i += 4;
                    }
                }
                else if(buf[i] == (uint8_t)0xe9)
                {
                    uint16_t val;
                    memcpy(&val, &buf[i + 1], 2);
                    val -= (uint16_t)pos;
                    memcpy(&buf[i + 1], &val, 2);
                    i += 2;
                }
            }

            for(; i < r; i++)
            {
                if(buf[i] == (uint8_t)0xe8 || buf[i] == (uint8_t)0xe9)
                {
                    ctx->prep_num_kept_bytes = r - i;
                    memcpy(ctx->prep_kept_bytes_buf, &buf[i], ctx->prep_num_kept_bytes);
                    break;
                }
            }
        }

        return r - ctx->prep_num_kept_bytes;
    }
}

int ace_decompress_v2(ace_decompress_ctx_t *ctx, const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                      size_t *out_len)
{
    size_t orig_size = *out_len;
    size_t total_out = 0;
    int    r;

    ctx->in_buf  = in_buf;
    ctx->in_len  = in_len;
    ctx->in_pos  = 0;
    ctx->error   = 0;
    ctx->dic_pos = 0;

    ctx->file_size = (int64_t)orig_size;
    ctx->file_pos  = 0;

    ctx->block_size          = 0;
    ctx->part_size           = 0;
    ctx->main_buf_pos        = 0;
    ctx->old_dists_pos       = 0;
    ctx->prep_num_kept_bytes = 0;
    ctx->prep_last_delta     = 0;
    ctx->delta_block_size    = 0;
    ctx->next_delta_len      = 0;
    memset(ctx->old_dists, 0, sizeof(ctx->old_dists));

    ctx->type      = ACE_BLOCK_LZ77_NORM;
    ctx->next_type = ACE_BLOCK_LZ77_NORM;

    ace_init_read_buf(ctx);

    while(total_out < orig_size && !ctx->error)
    {
        int remaining = (int)(orig_size - total_out);

        r = ace_decompress_v20_block(ctx, out_buf + total_out, remaining);
        if(r <= 0) break;
        total_out += r;
    }

    *out_len = total_out;

    ace_pic_done(ctx);

    return ctx->error ? -1 : 0;
}
