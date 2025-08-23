/*
* This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2025 Natalia Portillo.
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

// Lempel-Ziv-Davis compression implementation based on the public domain code from
// Rahul Dhesi from zoo

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lzd.h"
#include "../library.h"

static void init_dict(LZDContext *ctx)
{
    ctx->nbits     = 9;
    ctx->max_code  = 1u << 9;
    ctx->free_code = FIRST_FREE;
    ctx->have_old  = 0;
}

static int firstch(LZDContext *ctx, int code)
{
    int steps = 0;
    while(code > 255)
    {
        if((unsigned)code > MAXMAX) return -1;
        if(++steps > (int)MAXMAX) return -1;
        code = ctx->head[code];
    }
    return code;
}

static int fill_bits(LZDContext *ctx)
{
    while(ctx->bitcount < (int)ctx->nbits)
    {
        if(ctx->in_pos >= ctx->in_len) return -1;
        ctx->bitbuf |= (uint64_t)ctx->in_ptr[ctx->in_pos++] << ctx->bitcount;
        ctx->bitcount += 8;
    }
    return 0;
}

static int read_code(LZDContext *ctx)
{
    if(fill_bits(ctx) < 0) return -1;
    int code = (int)(ctx->bitbuf & masks[ctx->nbits]);
    ctx->bitbuf >>= ctx->nbits;
    ctx->bitcount -= ctx->nbits;
    return code;
}

LZDStatus LZD_Init(LZDContext *ctx)
{
    memset(ctx, 0, sizeof *ctx);
    ctx->head  = malloc((MAXMAX + 1) * sizeof *ctx->head);
    ctx->tail  = malloc((MAXMAX + 1) * sizeof *ctx->tail);
    ctx->stack = malloc((MAXMAX + 1) * sizeof *ctx->stack);
    if(!ctx->head || !ctx->tail || !ctx->stack) return LZD_NEED_INPUT;

    for(int i = 0; i < 256; i++)
    {
        ctx->head[i] = -1;
        ctx->tail[i] = (uint8_t)i;
    }
    ctx->stack_lim = ctx->stack + (MAXMAX + 1);
    ctx->stack_ptr = ctx->stack_lim;
    init_dict(ctx);
    ctx->bitbuf   = 0;
    ctx->bitcount = 0;
    return LZD_OK;
}

LZDStatus LZD_Feed(LZDContext *ctx, const unsigned char *in, size_t in_len)
{
    ctx->in_ptr = in;
    ctx->in_len = in_len;
    ctx->in_pos = 0;
    return LZD_OK;
}

LZDStatus LZD_Drain(LZDContext *ctx, unsigned char *out, size_t out_len, size_t *out_produced)
{
    size_t outpos = 0;

    while(outpos < out_len)
    {
        if(ctx->stack_ptr < ctx->stack_lim)
        {
            out[outpos++] = (uint8_t)*ctx->stack_ptr++;
            continue;
        }

        int raw = read_code(ctx);
        if(raw < 0)
        {
            *out_produced = outpos;
            return outpos > 0 ? LZD_OK : LZD_NEED_INPUT;
        }
        unsigned code = (unsigned)raw;

        if(code == CLEAR)
        {
            init_dict(ctx);
            int lit = read_code(ctx);
            if(lit < 0)
            {
                *out_produced = outpos;
                return outpos > 0 ? LZD_OK : LZD_NEED_INPUT;
            }
            ctx->old_code = (unsigned)lit;
            ctx->have_old = 1;
            out[outpos++] = (uint8_t)lit;
            continue;
        }
        if(code == Z_EOF)
        {
            *out_produced = outpos;
            return LZD_DONE;
        }

        unsigned in_code = code;
        if(code >= ctx->free_code)
        {
            if(!ctx->have_old) return LZD_DONE;
            int fc = firstch(ctx, ctx->old_code);
            if(fc < 0) return LZD_DONE;
            *--ctx->stack_ptr = (char)fc;
            code              = ctx->old_code;
        }

        while(code > 255)
        {
            *--ctx->stack_ptr = (char)ctx->tail[code];
            code              = ctx->head[code];
        }
        uint8_t first_byte = (uint8_t)code;
        *--ctx->stack_ptr  = (char)first_byte;

        if(ctx->have_old && ctx->free_code <= MAXMAX)
        {
            ctx->tail[ctx->free_code] = first_byte;
            ctx->head[ctx->free_code] = (int)ctx->old_code;
            ctx->free_code++;
            if(ctx->free_code >= ctx->max_code && ctx->nbits < MAXBITS)
            {
                ctx->nbits++;
                ctx->max_code <<= 1;
            }
        }
        ctx->old_code = in_code;
        ctx->have_old = 1;
    }

    *out_produced = outpos;
    return LZD_OK;
}

void LZD_Destroy(LZDContext *ctx)
{
    if(!ctx) return;
    free(ctx->head);
    free(ctx->tail);
    free(ctx->stack);
}

AARU_EXPORT void AARU_CALL *CreateLZDContext(void)
{
    LZDContext *c = malloc(sizeof *c);
    return c && LZD_Init(c) == LZD_OK ? c : (free(c), NULL);
}

AARU_EXPORT void AARU_CALL DestroyLZDContext(void *ctx)
{
    if(ctx)
    {
        LZD_Destroy(ctx);
        free(ctx);
    }
}

AARU_EXPORT int AARU_CALL LZD_FeedNative(void *ctx, const unsigned char *data, size_t length)
{
    return (int)LZD_Feed(ctx, data, length);
}

AARU_EXPORT int AARU_CALL LZD_DrainNative(void *ctx, unsigned char *outBuf, size_t outBufLen, size_t *produced)
{
    return (int)LZD_Drain(ctx, outBuf, outBufLen, produced);
}
