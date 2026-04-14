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

#include "reduce.h"

#include "../pak/bitstream.h"

/* DLE escape byte used by ZIP Reduce */
#define REDUCE_DLE 0x90

/* Minimum number of bits needed to represent (n-1) */
static int reduce_b_value(int n)
{
    if(n > 16) return 5;
    if(n > 8) return 4;
    if(n > 4) return 3;
    if(n > 2) return 2;
    if(n > 0) return 1;
    return 0;
}

int zip_reduce_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len, int comp_factor)
{
    BitStream bs;
    uint8_t   follower_sets[256][32];
    uint8_t   follower_count[256];
    size_t    out_pos  = 0;
    size_t    out_size = *out_len;
    int       v_len_bits;
    int       v_len_mask;

    if(!in_buf || !out_buf || !out_len) return -1;
    if(comp_factor < 1 || comp_factor > 4) return -1;

    v_len_bits = 8 - comp_factor;
    v_len_mask = (1 << v_len_bits) - 1;

    bitstream_init(&bs, in_buf, in_len);

    /* Read follower sets in reverse order (255 down to 0) */
    for(int i = 255; i >= 0; i--)
    {
        follower_count[i] = (uint8_t)bitstream_read_bits_le(&bs, 6);

        if(follower_count[i] > 32)
        {
            *out_len = 0;
            return -1;
        }

        for(int j = 0; j < follower_count[i]; j++) follower_sets[i][j] = (uint8_t)bitstream_read_bits_le(&bs, 8);
    }

    /* Decode using follower sets + LZ77 state machine */
    uint8_t last_char = 0;
    int     state     = 0;
    uint8_t v_byte    = 0;
    size_t  match_len = 0;

    while(out_pos < out_size && !bitstream_eof(&bs))
    {
        /* Read next byte through follower sets */
        uint8_t c;

        if(follower_count[last_char] == 0) { c = (uint8_t)bitstream_read_bits_le(&bs, 8); }
        else
        {
            uint32_t flag = bitstream_read_bits_le(&bs, 1);

            if(flag == 1) { c = (uint8_t)bitstream_read_bits_le(&bs, 8); }
            else
            {
                int bw  = reduce_b_value(follower_count[last_char]);
                int idx = (int)bitstream_read_bits_le(&bs, bw);

                if(idx >= follower_count[last_char])
                {
                    *out_len = out_pos;
                    return -1;
                }

                c = follower_sets[last_char][idx];
            }
        }

        last_char = c;

        /* LZ77 state machine */
        switch(state)
        {
            case 0:
                if(c != REDUCE_DLE) { out_buf[out_pos++] = c; }
                else
                {
                    state = 1;
                }
                break;

            case 1:
                if(c != 0)
                {
                    v_byte    = c;
                    match_len = v_byte & v_len_mask;

                    if(match_len == (size_t)v_len_mask)
                        state = 2; /* Need extra length byte */
                    else
                        state = 3; /* Read distance byte next */
                }
                else
                {
                    /* Escaped DLE: output literal 0x90 */
                    out_buf[out_pos++] = REDUCE_DLE;
                    state              = 0;
                }
                break;

            case 2:
                match_len += c;
                state = 3;
                break;

            case 3:
            {
                size_t dist = ((size_t)(v_byte >> v_len_bits)) * 256 + c + 1;
                match_len += 3;

                for(size_t i = 0; i < match_len && out_pos < out_size; i++)
                {
                    if(dist > out_pos)
                        out_buf[out_pos] = 0; /* Before start of output = zeros */
                    else
                        out_buf[out_pos] = out_buf[out_pos - dist];

                    out_pos++;
                }

                state = 0;
                break;
            }
        }
    }

    *out_len = out_pos;
    return 0;
}
