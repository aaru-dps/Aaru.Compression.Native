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

#include "arj.h"
#include "../lha/bitio.h"
#include "../lha/huffman.h"
#include "../lha/lzss.h"

#include <stdlib.h>
#include <string.h>

/*
 * Parse a generic Huffman code from the bitstream.
 * width = number of bits for the symbol count.
 * specialindex = index at which to insert extra zero-run bits (-1 for none).
 */
static lha_prefix_code *arj_parse_code(lha_bitio *bitio, int width, int specialindex)
{
    int num = (int)lha_bitio_next_bits(bitio, width);

    if(num == 0)
    {
        int              val  = (int)lha_bitio_next_bits(bitio, width);
        lha_prefix_code *code = lha_prefix_code_new();

        if(!code) return NULL;

        lha_prefix_code_add(code, val, 0, 0);
        return code;
    }
    else
    {
        int             *codelengths = (int *)calloc((size_t)num, sizeof(int));
        int              n           = 0;
        lha_prefix_code *code;

        if(!codelengths) return NULL;

        while(n < num)
        {
            int len = (int)lha_bitio_next_bits(bitio, 3);

            if(len == 7)
                while(lha_bitio_next_bit(bitio)) len++;

            codelengths[n++] = len;

            if(n == specialindex)
            {
                int zeroes = (int)lha_bitio_next_bits(bitio, 2);
                int i;

                for(i = 0; i < zeroes && n < num; i++) codelengths[n++] = 0;
            }
        }

        code = lha_prefix_code_from_lengths(codelengths, num, 16, true);
        free(codelengths);
        return code;
    }
}

/*
 * Parse the literal/length Huffman code (uses metacode compression).
 */
static lha_prefix_code *arj_parse_literal_code(lha_bitio *bitio)
{
    lha_prefix_code *metacode = arj_parse_code(bitio, 5, 3);
    lha_prefix_code *code;
    int              num, n;
    int             *codelengths;

    if(!metacode) return NULL;

    num = (int)lha_bitio_next_bits(bitio, 9);

    if(num == 0)
    {
        int val = (int)lha_bitio_next_bits(bitio, 9);

        lha_prefix_code_free(metacode);
        code = lha_prefix_code_new();

        if(!code) return NULL;

        lha_prefix_code_add(code, val, 0, 0);
        return code;
    }

    codelengths = (int *)calloc((size_t)num, sizeof(int));

    if(!codelengths)
    {
        lha_prefix_code_free(metacode);
        return NULL;
    }

    n = 0;

    while(n < num)
    {
        int c = lha_prefix_code_decode(bitio, metacode);

        if(c < 0) break;

        if(c <= 2)
        {
            int zeros = 0, i;

            switch(c)
            {
                case 0:
                    zeros = 1;
                    break;
                case 1:
                    zeros = (int)lha_bitio_next_bits(bitio, 4) + 3;
                    break;
                case 2:
                    zeros = (int)lha_bitio_next_bits(bitio, 9) + 20;
                    break;
            }

            if(n + zeros > num) zeros = num - n;

            for(i = 0; i < zeros; i++) codelengths[n++] = 0;
        }
        else
        {
            codelengths[n++] = c - 2;
        }
    }

    lha_prefix_code_free(metacode);
    code = lha_prefix_code_from_lengths(codelengths, num, 16, true);
    free(codelengths);
    return code;
}

/*
 * Block-based static Huffman + LZSS decompression for ARJ methods 1-3.
 * window_bits: 15 for standard ARJ, 16 for ARJZ version 51.
 */
static int arj_decompress_lzh(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len, int window_bits)
{
    lha_bitio        bitio;
    lha_lzss         lzss;
    lha_prefix_code *literalcode  = NULL;
    lha_prefix_code *distancecode = NULL;
    int              blocksize    = 0;
    int              blockpos     = 0;
    size_t           expected;
    int              dist_width;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    expected   = *out_len;
    dist_width = window_bits < 15 ? 4 : 5;

    if(!lha_lzss_init(&lzss, (size_t)1 << window_bits, out_buf, expected)) return -1;

    lha_bitio_init(&bitio, in_buf, in_len);

    while(lzss.out_pos < expected && !lha_bitio_at_eof(&bitio))
    {
        if(blockpos >= blocksize)
        {
            blocksize = (int)lha_bitio_next_bits(&bitio, 16);
            blockpos  = 0;

            lha_prefix_code_free(literalcode);
            lha_prefix_code_free(distancecode);

            literalcode  = arj_parse_literal_code(&bitio);
            distancecode = arj_parse_code(&bitio, dist_width, -1);

            if(!literalcode || !distancecode)
            {
                lha_prefix_code_free(literalcode);
                lha_prefix_code_free(distancecode);
                lha_lzss_cleanup(&lzss);
                return -1;
            }
        }

        blockpos++;

        {
            int lit = lha_prefix_code_decode(&bitio, literalcode);

            if(lit < 0) break;

            if(lit < 0x100) { lha_lzss_emit_literal(&lzss, (uint8_t)lit); }
            else
            {
                int length = lit - 0x100 + 3;
                int bit    = lha_prefix_code_decode(&bitio, distancecode);
                int offset;

                if(bit < 0) break;

                if(bit == 0)
                    offset = 1;
                else if(bit == 1)
                    offset = 2;
                else
                    offset = (1 << (bit - 1)) + (int)lha_bitio_next_bits(&bitio, bit - 1) + 1;

                lha_lzss_emit_match(&lzss, offset, length);
            }
        }
    }

    *out_len = lzss.out_pos;
    lha_prefix_code_free(literalcode);
    lha_prefix_code_free(distancecode);
    lha_lzss_cleanup(&lzss);
    return 0;
}

AARU_EXPORT int AARU_CALL arj_decompress_method1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len)
{ return arj_decompress_lzh(in_buf, in_len, out_buf, out_len, 15); }

AARU_EXPORT int AARU_CALL arj_decompress_method2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len)
{ return arj_decompress_lzh(in_buf, in_len, out_buf, out_len, 15); }

AARU_EXPORT int AARU_CALL arj_decompress_method3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len)
{ return arj_decompress_lzh(in_buf, in_len, out_buf, out_len, 15); }

AARU_EXPORT int AARU_CALL arjz_decompress_method1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len)
{ return arj_decompress_lzh(in_buf, in_len, out_buf, out_len, 16); }

AARU_EXPORT int AARU_CALL arjz_decompress_method2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len)
{ return arj_decompress_lzh(in_buf, in_len, out_buf, out_len, 16); }

AARU_EXPORT int AARU_CALL arjz_decompress_method3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len)
{ return arj_decompress_lzh(in_buf, in_len, out_buf, out_len, 16); }
