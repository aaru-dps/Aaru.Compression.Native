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

/* StuffIt X method 2: Darkhorse (Context-weighted LZSS + Range Coder) */

#include <stdlib.h>
#include <string.h>
#include "rangecoder.h"
#include "stuffit.h"

static int dh_next_bit_with_weight(StuffitRangeCoder *c, uint32_t *weight)
{
    int bit = stuffit_rc_next_weighted_bit2(c, *weight, 12);
    if(bit == 0)
        *weight += (0x1000 - *weight) >> 5;
    else
        *weight -= *weight >> 5;
    return bit;
}

static int dh_read_symbol(StuffitRangeCoder *c, uint32_t *weights, int num_bits)
{
    int val = 1;
    for(int i = 0; i < num_bits; i++) val = (val << 1) | dh_next_bit_with_weight(c, &weights[val]);
    return val - (1 << num_bits);
}

static const int dh_offset_table[64] = {
    0,         1,         2,         3,         4,         6,         8,          0xc,        0x10,       0x18,
    0x20,      0x30,      0x40,      0x60,      0x80,      0xc0,      0x100,      0x180,      0x200,      0x300,
    0x400,     0x600,     0x800,     0xc00,     0x1000,    0x1800,    0x2000,     0x3000,     0x4000,     0x6000,
    0x8000,    0xc000,    0x10000,   0x18000,   0x20000,   0x30000,   0x40000,    0x60000,    0x80000,    0xc0000,
    0x100000,  0x180000,  0x200000,  0x300000,  0x400000,  0x600000,  0x800000,   0xc00000,   0x1000000,  0x1800000,
    0x2000000, 0x3000000, 0x4000000, 0x6000000, 0x8000000, 0xc000000, 0x10000000, 0x18000000, 0x20000000, 0x30000000,
    0,         0,         0,         0,
};
static const int dh_bitlen_table[64] = {0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
                                        7,  7,  8,  8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
                                        15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
                                        23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 0,  0,  0,  0};

int stuffitx_darkhorse_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size,
                                     int window_bits)
{
    size_t limit       = *dst_size;
    size_t di          = 0;
    int    window_size = 1 << window_bits;
    if(window_size < 0x100000) window_size = 0x100000;

    uint8_t *window = calloc(window_size, 1);
    if(!window) return -1;

    uint32_t flagweights[4], flagweight2;
    uint32_t litweights[16][256], litweights2[16][256][2];
    uint32_t recencyweight1, recencyweight2, recencyweight3;
    uint32_t recencyweights[4], lenweight;
    uint32_t shortweights[4][16], longweights[256];
    uint32_t distlenweights[4][64], distweights[10][32], distlowbitweights[16];
    int      distancetable[4];

    int next = -1;

    for(int i = 0; i < 4; i++) flagweights[i] = 0x800;
    flagweight2 = 0x800;
    for(int i = 0; i < 16; i++)
        for(int j = 0; j < 256; j++)
        {
            litweights[i][j]     = 0x800;
            litweights2[i][j][0] = 0x800;
            litweights2[i][j][1] = 0x800;
        }
    recencyweight1 = recencyweight2 = recencyweight3 = 0x800;
    for(int i = 0; i < 4; i++) recencyweights[i] = 0x800;
    lenweight = 0x800;
    for(int i = 0; i < 4; i++)
        for(int j = 0; j < 16; j++) shortweights[i][j] = 0x800;
    for(int i = 0; i < 256; i++) longweights[i] = 0x800;
    for(int i = 0; i < 4; i++)
        for(int j = 0; j < 64; j++) distlenweights[i][j] = 0x800;
    for(int i = 0; i < 10; i++)
        for(int j = 0; j < 32; j++) distweights[i][j] = 0x800;
    for(int i = 0; i < 16; i++) distlowbitweights[i] = 0x800;
    memset(distancetable, 0, sizeof(distancetable));

    /* Skip 1 initial byte, then initialize range coder */
    size_t            start = 1;
    StuffitRangeCoder coder;
    stuffit_rc_init(&coder, src + start, src_size - start, NULL, false, 0);

    int win_pos = 0;

    while(di < limit)
    {
        if(dh_next_bit_with_weight(&coder, &flagweights[di & 3]) == 0)
        {
            /* Literal */
            int prev = (di > 0) ? window[(win_pos - 1 + window_size) & (window_size - 1)] : 0;
            int val  = 1;

            if(next == -1)
            {
                while(val < 0x100) val = (val << 1) | dh_next_bit_with_weight(&coder, &litweights[prev / 16][val]);
            }
            else
            {
                int guess = next;
                while(val < 0x100)
                {
                    int bit = dh_next_bit_with_weight(&coder, &litweights2[prev / 16][val][(guess >> 7) & 1]);
                    val     = (val << 1) | bit;
                    if(bit != ((guess >> 7) & 1)) break;
                    guess <<= 1;
                }
                while(val < 0x100) val = (val << 1) | dh_next_bit_with_weight(&coder, &litweights[prev / 16][val]);
            }

            uint8_t byte    = val & 0xff;
            dst[di++]       = byte;
            window[win_pos] = byte;
            win_pos         = (win_pos + 1) & (window_size - 1);
            next            = -1;
        }
        else
        {
            /* Match */
            int len, offs;

            if(dh_next_bit_with_weight(&coder, &flagweight2) == 0)
            {
                /* New distance */
                if(dh_next_bit_with_weight(&coder, &lenweight) == 0)
                    len = dh_read_symbol(&coder, shortweights[di & 3], 4);
                else
                    len = dh_read_symbol(&coder, longweights, 8) + 16;
                len += 2;

                if(len == 0x111) break; /* end marker */

                /* Read distance */
                int dl = len - 2;
                if(dl > 3) dl = 3;
                int sym = dh_read_symbol(&coder, distlenweights[dl], 6);

                if(sym < 4)
                    offs = sym;
                else if(sym < 14)
                    offs = dh_offset_table[sym] + dh_read_symbol(&coder, distweights[sym - 4], dh_bitlen_table[sym]);
                else
                {
                    int numbits = dh_bitlen_table[sym];
                    int val     = 0;
                    for(int i = numbits - 1; i >= 4; i--) val |= stuffit_rc_next_bit(&coder) << i;
                    offs = val + dh_offset_table[sym] + dh_read_symbol(&coder, distlowbitweights, 4);
                }

                /* Update distance memory */
                for(int i = 3; i > 0; i--) distancetable[i] = distancetable[i - 1];
                distancetable[0] = offs;
            }
            else
            {
                /* Recency */
                int recency;
                if(dh_next_bit_with_weight(&coder, &recencyweight1) == 0)
                {
                    if(dh_next_bit_with_weight(&coder, &recencyweights[di & 3]) == 0)
                    {
                        offs = distancetable[0];
                        len  = 1;
                        goto emit;
                    }
                    else
                        recency = 0;
                }
                else
                {
                    if(dh_next_bit_with_weight(&coder, &recencyweight2) == 0)
                        recency = 1;
                    else if(dh_next_bit_with_weight(&coder, &recencyweight3) == 0)
                        recency = 2;
                    else
                        recency = 3;
                }

                offs = distancetable[recency];
                for(int i = recency; i > 0; i--) distancetable[i] = distancetable[i - 1];
                distancetable[0] = offs;

                if(dh_next_bit_with_weight(&coder, &lenweight) == 0)
                    len = dh_read_symbol(&coder, shortweights[di & 3], 4);
                else
                    len = dh_read_symbol(&coder, longweights, 8) + 16;
                len += 2;
            }

        emit:
            /* Copy from window */
            {
                int match_start = win_pos; /* position before copy */
                int copy_src    = (win_pos - offs - 1 + window_size) & (window_size - 1);
                for(int j = 0; j < len && di < limit; j++)
                {
                    uint8_t b       = window[(copy_src + j) & (window_size - 1)];
                    dst[di++]       = b;
                    window[win_pos] = b;
                    win_pos         = (win_pos + 1) & (window_size - 1);
                }
            }

            /* next byte prediction uses position BEFORE the match was written */
            next = window[(win_pos - len - offs - 1 + len % (offs + 1) + window_size * 2) & (window_size - 1)];
        }
    }

    free(window);
    *dst_size = di;
    return 0;
}
