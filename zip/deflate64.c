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

#include "deflate64.h"

#include <stdlib.h>
#include <string.h>

#include "../pak/bitstream.h"
#include "../pak/prefixcode.h"

/* Deflate64 window size: 64KB */
#define DEFLATE64_WINDOW_SIZE 65536
#define DEFLATE64_WINDOW_MASK (DEFLATE64_WINDOW_SIZE - 1)

/* Block types */
#define BLOCK_STORED  0
#define BLOCK_FIXED   1
#define BLOCK_DYNAMIC 2

/* Number of symbols */
#define NUM_LITLEN_SYMBOLS 288
#define NUM_DIST_SYMBOLS   32
#define MAX_LITLEN_SYMBOLS 286 /* Only 0-285 are defined for Deflate64 */

/* Length base values for codes 257-285 */
static const int length_base[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 3};

/* Length extra bits for codes 257-285 (Deflate64: code 285 has 16 extra bits) */
static const int length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                     2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 16};

/* Distance base values for codes 0-31 */
static const int dist_base[32] = {1,    2,    3,    4,    5,    7,     9,     13,    17,    25,   33,
                                  49,   65,   97,   129,  193,  257,   385,   513,   769,   1025, 1537,
                                  2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 32769, 49153};

/* Distance extra bits for codes 0-31 */
static const int dist_extra[32] = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
                                   7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

/* Order of code length codes for dynamic Huffman tables */
static const int codelen_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* Build static fixed Huffman tables for Deflate */
static PrefixCode *build_fixed_litlen_code(void)
{
    int lengths[NUM_LITLEN_SYMBOLS];
    int i;

    for(i = 0; i <= 143; i++) lengths[i] = 8;
    for(i = 144; i <= 255; i++) lengths[i] = 9;
    for(i = 256; i <= 279; i++) lengths[i] = 7;
    for(i = 280; i <= 287; i++) lengths[i] = 8;

    return prefix_code_alloc_with_lengths(lengths, NUM_LITLEN_SYMBOLS, 9, true);
}

static PrefixCode *build_fixed_dist_code(void)
{
    int lengths[NUM_DIST_SYMBOLS];

    for(int i = 0; i < NUM_DIST_SYMBOLS; i++) lengths[i] = 5;

    return prefix_code_alloc_with_lengths(lengths, NUM_DIST_SYMBOLS, 5, true);
}

/* Build dynamic Huffman tables from the stream */
static int build_dynamic_tables(BitStream *bs, PrefixCode **litlen_code_out, PrefixCode **dist_code_out)
{
    int hlit  = (int)bitstream_read_bits_le(bs, 5) + 257;
    int hdist = (int)bitstream_read_bits_le(bs, 5) + 1;
    int hclen = (int)bitstream_read_bits_le(bs, 4) + 4;

    /* Read code length code lengths */
    int codelen_lengths[19];
    memset(codelen_lengths, 0, sizeof(codelen_lengths));

    for(int i = 0; i < hclen; i++) codelen_lengths[codelen_order[i]] = (int)bitstream_read_bits_le(bs, 3);

    /* Build code length code */
    PrefixCode *codelen_code = prefix_code_alloc_with_lengths(codelen_lengths, 19, 7, true);

    if(!codelen_code) return -1;

    /* Read literal/length and distance code lengths */
    int  total_codes = hlit + hdist;
    int *all_lengths = (int *)calloc(total_codes, sizeof(int));

    if(!all_lengths)
    {
        prefix_code_free(codelen_code);
        return -1;
    }

    int idx = 0;

    while(idx < total_codes)
    {
        int sym = prefix_code_read_symbol_le(bs, codelen_code);

        if(sym < 0)
        {
            free(all_lengths);
            prefix_code_free(codelen_code);
            return -1;
        }

        if(sym < 16)
        {
            /* Literal length value */
            all_lengths[idx++] = sym;
        }
        else if(sym == 16)
        {
            /* Repeat previous length 3-6 times */
            int repeat = (int)bitstream_read_bits_le(bs, 2) + 3;
            int prev   = idx > 0 ? all_lengths[idx - 1] : 0;

            for(int i = 0; i < repeat && idx < total_codes; i++) all_lengths[idx++] = prev;
        }
        else if(sym == 17)
        {
            /* Repeat zero 3-10 times */
            int repeat = (int)bitstream_read_bits_le(bs, 3) + 3;

            for(int i = 0; i < repeat && idx < total_codes; i++) all_lengths[idx++] = 0;
        }
        else if(sym == 18)
        {
            /* Repeat zero 11-138 times */
            int repeat = (int)bitstream_read_bits_le(bs, 7) + 11;

            for(int i = 0; i < repeat && idx < total_codes; i++) all_lengths[idx++] = 0;
        }
    }

    prefix_code_free(codelen_code);

    /* Find max code lengths */
    int max_litlen = 0;

    for(int i = 0; i < hlit; i++)
    {
        if(all_lengths[i] > max_litlen) max_litlen = all_lengths[i];
    }

    if(max_litlen == 0) max_litlen = 1;

    int max_dist = 0;

    for(int i = hlit; i < total_codes; i++)
    {
        if(all_lengths[i] > max_dist) max_dist = all_lengths[i];
    }

    if(max_dist == 0) max_dist = 1;

    /* Build literal/length code */
    *litlen_code_out = prefix_code_alloc_with_lengths(all_lengths, hlit, max_litlen, true);

    /* Build distance code */
    *dist_code_out = prefix_code_alloc_with_lengths(all_lengths + hlit, hdist, max_dist, true);

    free(all_lengths);

    if(!*litlen_code_out || !*dist_code_out)
    {
        if(*litlen_code_out)
        {
            prefix_code_free(*litlen_code_out);
            *litlen_code_out = NULL;
        }

        if(*dist_code_out)
        {
            prefix_code_free(*dist_code_out);
            *dist_code_out = NULL;
        }

        return -1;
    }

    return 0;
}

int zip_deflate64_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    BitStream bs;
    size_t    out_pos  = 0;
    size_t    out_size = *out_len;
    int       result   = 0;
    int       is_final = 0;

    if(!in_buf || !out_buf || !out_len) return -1;

    bitstream_init(&bs, in_buf, in_len);

    while(!is_final && out_pos < out_size)
    {
        PrefixCode *litlen_code = NULL;
        PrefixCode *dist_code   = NULL;
        int         free_codes  = 0;

        is_final       = (int)bitstream_read_bits_le(&bs, 1);
        int block_type = (int)bitstream_read_bits_le(&bs, 2);

        if(block_type == BLOCK_STORED)
        {
            /* Skip to byte boundary */
            if(bs.bitcount > 0)
            {
                bs.bitbuffer = 0;
                bs.bitcount  = 0;
            }

            uint16_t len  = bitstream_read_uint16_le(&bs);
            uint16_t nlen = bitstream_read_uint16_le(&bs);

            (void)nlen; /* Complement check can be skipped; we trust the data */

            for(uint16_t i = 0; i < len && out_pos < out_size; i++)
            {
                if(bs.pos >= bs.length) break;

                out_buf[out_pos++] = bs.data[bs.pos++];
            }

            continue;
        }
        else if(block_type == BLOCK_FIXED)
        {
            litlen_code = build_fixed_litlen_code();
            dist_code   = build_fixed_dist_code();
            free_codes  = 1;

            if(!litlen_code || !dist_code)
            {
                result = -1;
                goto block_cleanup;
            }
        }
        else if(block_type == BLOCK_DYNAMIC)
        {
            if(build_dynamic_tables(&bs, &litlen_code, &dist_code) != 0)
            {
                result = -1;
                goto block_cleanup;
            }

            free_codes = 1;
        }
        else
        {
            result = -1;
            break;
        }

        /* Decode symbols */
        while(out_pos < out_size)
        {
            int sym = prefix_code_read_symbol_le(&bs, litlen_code);

            if(sym < 0)
            {
                result = -1;
                goto block_cleanup;
            }

            if(sym < 256)
            {
                /* Literal byte */
                out_buf[out_pos++] = (uint8_t)sym;
            }
            else if(sym == 256)
            {
                /* End of block */
                break;
            }
            else
            {
                /* Length/distance pair */
                int length_code_idx = sym - 257;

                if(length_code_idx < 0 || length_code_idx >= 29)
                {
                    result = -1;
                    goto block_cleanup;
                }

                int length = length_base[length_code_idx];

                if(length_extra[length_code_idx] > 0)
                    length += (int)bitstream_read_bits_le(&bs, length_extra[length_code_idx]);

                /* Read distance */
                int dist_sym = prefix_code_read_symbol_le(&bs, dist_code);

                if(dist_sym < 0 || dist_sym >= 32)
                {
                    result = -1;
                    goto block_cleanup;
                }

                int distance = dist_base[dist_sym];

                if(dist_extra[dist_sym] > 0) distance += (int)bitstream_read_bits_le(&bs, dist_extra[dist_sym]);

                /* Copy from history */
                for(int i = 0; i < length && out_pos < out_size; i++)
                {
                    if((size_t)distance > out_pos)
                        out_buf[out_pos] = 0;
                    else
                        out_buf[out_pos] = out_buf[out_pos - distance];

                    out_pos++;
                }
            }
        }

    block_cleanup:
        if(free_codes)
        {
            if(litlen_code) prefix_code_free(litlen_code);
            if(dist_code) prefix_code_free(dist_code);
        }

        if(result != 0) break;
    }

    *out_len = out_pos;
    return result;
}
