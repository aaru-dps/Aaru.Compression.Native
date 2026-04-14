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

#include "../lha/bitio.h"
#include "../lha/lzss.h"
#include "arj.h"

#include <stdlib.h>

#define ARJ_FASTEST_WINDOW_SIZE 32768
#define ARJ_FASTEST_LEN_START   0
#define ARJ_FASTEST_LEN_STOP    7
#define ARJ_FASTEST_PTR_START   9
#define ARJ_FASTEST_PTR_STOP    13

/*
 * Decode a variable-width length value.
 * Reads continuation bits from width LEN_START to LEN_STOP.
 * Returns 0 for literal, >0 for match length component.
 */
static int arj_decode_len(lha_bitio *bitio)
{
    int val = 0;
    int w   = ARJ_FASTEST_LEN_START;

    while(w < ARJ_FASTEST_LEN_STOP)
    {
        if(!lha_bitio_next_bit(bitio)) break;

        val += 1 << w;
        w++;
    }

    if(w > ARJ_FASTEST_LEN_START) val += (int)lha_bitio_next_bits(bitio, w);

    return val;
}

/*
 * Decode a variable-width distance/pointer value.
 * Reads continuation bits from width PTR_START to PTR_STOP.
 */
static int arj_decode_ptr(lha_bitio *bitio)
{
    int val = 0;
    int w   = ARJ_FASTEST_PTR_START;

    while(w < ARJ_FASTEST_PTR_STOP)
    {
        if(!lha_bitio_next_bit(bitio)) break;

        val += 1 << w;
        w++;
    }

    val += (int)lha_bitio_next_bits(bitio, w);
    return val;
}

/*
 * ARJ method 4 ("Fastest") decompression.
 * LZSS with variable-width Golomb-Rice codes for length and distance.
 * 32KB sliding window, MSB-first bit extraction.
 */
AARU_EXPORT int AARU_CALL arj_decompress_fastest(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len)
{
    lha_bitio bitio;
    lha_lzss  lzss;
    size_t    expected;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    expected = *out_len;

    if(!lha_lzss_init(&lzss, ARJ_FASTEST_WINDOW_SIZE, out_buf, expected)) return -1;

    lha_bitio_init(&bitio, in_buf, in_len);

    while(lzss.out_pos < expected && !lha_bitio_at_eof(&bitio))
    {
        int c = arj_decode_len(&bitio);

        if(c == 0)
        {
            /* Literal byte */
            uint8_t byte = (uint8_t)lha_bitio_next_bits(&bitio, 8);
            lha_lzss_emit_literal(&lzss, byte);
        }
        else
        {
            /* Match: length = c - 1 + threshold(3) = c + 2 */
            int length = c + 2;
            int offset = arj_decode_ptr(&bitio) + 1;

            lha_lzss_emit_match(&lzss, offset, length);
        }
    }

    *out_len = lzss.out_pos;
    lha_lzss_cleanup(&lzss);
    return 0;
}
