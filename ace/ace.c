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

#include <stdlib.h>
#include <string.h>

#include "ace.h"
#include "ace_internal.h"

void ace_init_bitwidth(ace_decompress_ctx_t *ctx)
{
    int i, k, j;

    i = k = 0;
    j     = 1;

    do
    {
        if(i == j)
        {
            k++;
            j <<= 1;
        }
        ctx->bit_width_array[i++] = (uint8_t)k;
    } while(i < ACE_MAX_DIST2);

    for(i = -128; i <= 127; i++)
    {
        if(i < 0)
            ctx->dif_bit_width_array[(uint8_t)i] = ctx->bit_width_array[-2 * i - 1];
        else
            ctx->dif_bit_width_array[(uint8_t)i] = ctx->bit_width_array[2 * i];
    }
}

int ace_get_bit_width(ace_decompress_ctx_t *ctx, int value)
{
    return value < ACE_MAX_DIST2 ? ctx->bit_width_array[value]
                                 : ctx->bit_width_array[value >> ACE_MAX_DIC_BITS2] + ACE_MAX_DIC_BITS2;
}

int ace_decompress_init(ace_decompress_ctx_t *ctx, int dic_bits)
{
    memset(ctx, 0, sizeof(*ctx));

    if(dic_bits < 10) dic_bits = 10;
    if(dic_bits > ACE_MAX_DIC_BITS) dic_bits = ACE_MAX_DIC_BITS;

    ctx->dic_bits = dic_bits;
    ctx->dic_size = 1u << dic_bits;
    ctx->dic_and  = ctx->dic_size - 1;

    ctx->dictionary = (uint8_t *)malloc(ctx->dic_size);
    if(!ctx->dictionary) return -1;
    memset(ctx->dictionary, 0, ctx->dic_size);

    ace_init_bitwidth(ctx);

    {
        int i;
        for(i = 0; i <= 128; i++)
        {
            ctx->sound_quantizer[256 - i] = ctx->sound_quantizer[i] = ace_get_bit_width(ctx, i);
        }
    }

    return 0;
}

void ace_decompress_free(ace_decompress_ctx_t *ctx)
{
    if(ctx->dictionary)
    {
        free(ctx->dictionary);
        ctx->dictionary = NULL;
    }
    ace_pic_done(ctx);
}
