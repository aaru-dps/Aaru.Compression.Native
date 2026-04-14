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

static void ace_qsort_xchg(uint16_t *a, uint16_t *b)
{
    uint16_t tmp = *a;
    *a           = *b;
    *b           = tmp;
}

static void ace_qsort_range(ace_decompress_ctx_t *ctx, int left, int right)
{
    int      new_left, new_right;
    uint16_t hyphen;

    new_left  = left;
    new_right = right;
    hyphen    = ctx->sort_frequencies[right];

    do
    {
        while(ctx->sort_frequencies[new_left] > hyphen) new_left++;
        while(ctx->sort_frequencies[new_right] < hyphen) new_right--;

        if(new_left <= new_right)
        {
            ace_qsort_xchg(&ctx->sort_frequencies[new_left], &ctx->sort_frequencies[new_right]);
            ace_qsort_xchg(&ctx->sort_elements[new_left], &ctx->sort_elements[new_right]);
            new_left++;
            new_right--;
        }
    } while(new_left < new_right);

    if(left < new_right)
    {
        if(left < new_right - 1)
            ace_qsort_range(ctx, left, new_right);
        else if(ctx->sort_frequencies[left] < ctx->sort_frequencies[new_right])
        {
            ace_qsort_xchg(&ctx->sort_frequencies[left], &ctx->sort_frequencies[new_right]);
            ace_qsort_xchg(&ctx->sort_elements[left], &ctx->sort_elements[new_right]);
        }
    }

    if(right > new_left)
    {
        if(new_left < right - 1)
            ace_qsort_range(ctx, new_left, right);
        else if(ctx->sort_frequencies[new_left] < ctx->sort_frequencies[right])
        {
            ace_qsort_xchg(&ctx->sort_frequencies[new_left], &ctx->sort_frequencies[right]);
            ace_qsort_xchg(&ctx->sort_elements[new_left], &ctx->sort_elements[right]);
        }
    }
}

static void ace_quicksort(ace_decompress_ctx_t *ctx, int n)
{
    int i;
    for(i = n + 1; i--;) ctx->sort_elements[i] = (uint16_t)i;
    ace_qsort_range(ctx, 0, n);
}

static void ace_memset16(uint16_t *buf, uint16_t val, int count)
{
    while(count--) *buf++ = val;
}

int ace_make_codes(ace_decompress_ctx_t *ctx, unsigned max_width, unsigned tab_size, uint16_t *widths, uint16_t *codes)
{
    unsigned num_codes, code, code_pos, i, max_code_pos, actual_size;

    memcpy(ctx->sort_frequencies, widths, (tab_size + 1) * sizeof(uint16_t));

    if(tab_size)
        ace_quicksort(ctx, (int)tab_size);
    else
        ctx->sort_elements[0] = 0;

    ctx->sort_frequencies[tab_size + 1] = actual_size = code_pos = 0;

    while(ctx->sort_frequencies[actual_size]) actual_size++;

    if(actual_size < 2)
    {
        i         = ctx->sort_elements[0];
        widths[i] = 1;
        actual_size += (actual_size == 0);
    }
    actual_size--;

    max_code_pos = 1u << max_width;
    for(i = actual_size + 1; i-- && code_pos < max_code_pos;)
    {
        num_codes = 1u << (max_width - ctx->sort_frequencies[i]);
        code      = ctx->sort_elements[i];
        if(code_pos + num_codes > max_code_pos) return 0;
        ace_memset16(&codes[code_pos], (uint16_t)code, (int)num_codes);
        code_pos += num_codes;
    }

    return 1;
}

int ace_read_widths(ace_decompress_ctx_t *ctx, unsigned max_width, uint16_t *codes, uint16_t *widths, unsigned max_size)
{
    unsigned code, i, width_pos, num_widths, len, upper_width, lower_width;

    memset(widths, 0, max_size * sizeof(uint16_t));
    memset(codes, 0, (1u << max_width) * sizeof(uint16_t));

    num_widths = ctx->read_code >> (32 - 9);
    ace_add_bits(ctx, 9);
    if(num_widths > max_size) num_widths = max_size;

    lower_width = ctx->read_code >> (32 - 4);
    ace_add_bits(ctx, 4);
    upper_width = ctx->read_code >> (32 - 4);
    ace_add_bits(ctx, 4);

    for(i = 0; i <= upper_width; i++)
    {
        ctx->save_widths[i] = ctx->read_code >> (32 - 3);
        ace_add_bits(ctx, 3);
    }

    if(!ace_make_codes(ctx, ACE_MAX_SAVE_WIDTH, upper_width, ctx->save_widths, codes)) return 0;

    width_pos = 0;
    while(width_pos <= num_widths)
    {
        code = codes[ctx->read_code >> (32 - ACE_MAX_SAVE_WIDTH)];
        ace_add_bits(ctx, ctx->save_widths[code]);

        if(code < upper_width)
            widths[width_pos++] = (uint16_t)code;
        else
        {
            len = (ctx->read_code >> 28) + 4;
            ace_add_bits(ctx, 4);
            while(len-- && width_pos <= num_widths) widths[width_pos++] = 0;
        }
    }

    if(upper_width)
    {
        for(i = 1; i <= num_widths; i++) widths[i] = (widths[i] + widths[i - 1]) % upper_width;
    }

    for(i = 0; i <= num_widths; i++)
    {
        if(widths[i]) widths[i] += (uint16_t)lower_width;
    }

    return ace_make_codes(ctx, max_width, num_widths, widths, codes);
}
