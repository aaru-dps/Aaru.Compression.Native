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

/* StuffIt X method 3: Modified Deflate (StuffIt X variant) */

#include <stdlib.h>
#include <string.h>
#include "../pak/bitstream.h"
#include "../pak/prefixcode.h"
#include "stuffit.h"
#include "stuffit_internal.h"

/* StuffIt X uses a different meta-table ordering for dynamic Huffman */
static const int sitx_deflate_meta_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* Standard Deflate length/distance base and extra bits tables */
static const int len_base[29]   = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const int len_extra[29]  = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                   2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const int dist_base[30]  = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                   33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                   1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const int dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                   6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

#define DEFLATE_WINDOW_SIZE 32768

int stuffitx_deflate_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    BitStream bs;
    bitstream_init(&bs, src, src_size);

    uint8_t *window = calloc(DEFLATE_WINDOW_SIZE, 1);
    if(!window) return -1;
    int win_pos    = 0;
    int last_block = 0;

    while(!last_block && di < limit)
    {
        last_block     = bitstream_read_bits_le(&bs, 1);
        int block_type = bitstream_read_bits_le(&bs, 2);

        if(block_type == 0)
        {
            /* Uncompressed block */
            bs.bitcount  = 0;
            bs.bitbuffer = 0; /* skip to byte boundary */
            int len      = bitstream_read_bits_le(&bs, 8) | (bitstream_read_bits_le(&bs, 8) << 8);
            /* skip nlen */
            bitstream_read_bits_le(&bs, 8);
            bitstream_read_bits_le(&bs, 8);
            for(int i = 0; i < len && di < limit; i++)
            {
                uint8_t b       = bitstream_read_bits_le(&bs, 8);
                dst[di++]       = b;
                window[win_pos] = b;
                win_pos         = (win_pos + 1) & (DEFLATE_WINDOW_SIZE - 1);
            }
        }
        else if(block_type == 1 || block_type == 2)
        {
            PrefixCode *lit_code = NULL, *dist_code = NULL;

            if(block_type == 1)
            {
                /* Fixed Huffman codes */
                int lengths[288];
                for(int i = 0; i <= 143; i++) lengths[i] = 8;
                for(int i = 144; i <= 255; i++) lengths[i] = 9;
                for(int i = 256; i <= 279; i++) lengths[i] = 7;
                for(int i = 280; i <= 287; i++) lengths[i] = 8;
                lit_code = prefix_code_alloc_with_lengths(lengths, 288, 15, true);

                int dlengths[30];
                for(int i = 0; i < 30; i++) dlengths[i] = 5;
                dist_code = prefix_code_alloc_with_lengths(dlengths, 30, 15, true);
            }
            else
            {
                /* Dynamic Huffman - StuffIt X uses 6 bits for distance count */
                int hlit  = bitstream_read_bits_le(&bs, 5) + 257;
                int hdist = bitstream_read_bits_le(&bs, 6) + 1;
                int hclen = bitstream_read_bits_le(&bs, 4) + 4;

                int cl_lengths[19] = {0};
                for(int i = 0; i < hclen; i++) cl_lengths[sitx_deflate_meta_order[i]] = bitstream_read_bits_le(&bs, 3);

                PrefixCode *cl_code = prefix_code_alloc_with_lengths(cl_lengths, 19, 7, true);
                if(!cl_code)
                {
                    free(window);
                    return -1;
                }

                int  total       = hlit + hdist;
                int *all_lengths = calloc(total, sizeof(int));
                if(!all_lengths)
                {
                    prefix_code_free(cl_code);
                    free(window);
                    return -1;
                }

                for(int i = 0; i < total;)
                {
                    int sym = prefix_code_read_symbol_le(&bs, cl_code);
                    if(sym < 0) break;
                    if(sym < 16)
                        all_lengths[i++] = sym;
                    else if(sym == 16)
                    {
                        int rep = bitstream_read_bits_le(&bs, 2) + 3;
                        int val = i > 0 ? all_lengths[i - 1] : 0;
                        for(int j = 0; j < rep && i < total; j++) all_lengths[i++] = val;
                    }
                    else if(sym == 17)
                    {
                        int rep = bitstream_read_bits_le(&bs, 3) + 3;
                        for(int j = 0; j < rep && i < total; j++) all_lengths[i++] = 0;
                    }
                    else if(sym == 18)
                    {
                        int rep = bitstream_read_bits_le(&bs, 7) + 11;
                        for(int j = 0; j < rep && i < total; j++) all_lengths[i++] = 0;
                    }
                }

                lit_code  = prefix_code_alloc_with_lengths(all_lengths, hlit, 15, true);
                dist_code = prefix_code_alloc_with_lengths(all_lengths + hlit, hdist, 15, true);

                free(all_lengths);
                prefix_code_free(cl_code);
            }

            if(!lit_code || !dist_code)
            {
                prefix_code_free(lit_code);
                prefix_code_free(dist_code);
                free(window);
                return -1;
            }

            /* Decompress block */
            for(;;)
            {
                int sym = prefix_code_read_symbol_le(&bs, lit_code);
                if(sym < 0 || sym == 256) break;

                if(sym < 256)
                {
                    if(di >= limit) break;
                    dst[di++]       = (uint8_t)sym;
                    window[win_pos] = (uint8_t)sym;
                    win_pos         = (win_pos + 1) & (DEFLATE_WINDOW_SIZE - 1);
                }
                else
                {
                    int len_idx = sym - 257;
                    if(len_idx < 0 || len_idx >= 29) break;
                    int length = len_base[len_idx] + bitstream_read_bits_le(&bs, len_extra[len_idx]);

                    int dist_sym = prefix_code_read_symbol_le(&bs, dist_code);
                    if(dist_sym < 0 || dist_sym >= 30) break;
                    int distance = dist_base[dist_sym] + bitstream_read_bits_le(&bs, dist_extra[dist_sym]);

                    for(int j = 0; j < length && di < limit; j++)
                    {
                        int     sp      = (win_pos - distance + DEFLATE_WINDOW_SIZE) & (DEFLATE_WINDOW_SIZE - 1);
                        uint8_t b       = window[sp];
                        dst[di++]       = b;
                        window[win_pos] = b;
                        win_pos         = (win_pos + 1) & (DEFLATE_WINDOW_SIZE - 1);
                    }
                }
            }

            prefix_code_free(lit_code);
            prefix_code_free(dist_code);
        }
        else
        {
            /* Invalid block type */
            free(window);
            return -1;
        }
    }

    free(window);
    *dst_size = di;
    return 0;
}
