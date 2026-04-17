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

/* StuffIt method 13: Dynamic/Static Huffman LZSS with 64KB window */

#include <stdlib.h>
#include <string.h>
#include "../pak/bitstream.h"
#include "../pak/prefixcode.h"
#include "stuffit.h"

#define M13_WINDOW_SIZE 65536
#define M13_CODE_SIZE   321

/* Meta-code table for dynamic mode (37 symbols, LSB-first) */
static const int meta_code_values[37]  = {0x5d8, 0x058, 0x040, 0x0c0, 0x000, 0x078, 0x02b, 0x014, 0x00c, 0x01c,
                                          0x01b, 0x00b, 0x010, 0x020, 0x038, 0x018, 0x0d8, 0xbd8, 0x180, 0x680,
                                          0x380, 0xf80, 0x780, 0x480, 0x080, 0x280, 0x3d8, 0xfd8, 0x7d8, 0x9d8,
                                          0x1d8, 0x004, 0x001, 0x002, 0x007, 0x003, 0x008};
static const int meta_code_lengths[37] = {11, 8,  8,  8,  8,  7,  6,  5,  5,  5,  5,  6,  5, 6, 7, 7, 9, 12, 10,
                                          11, 11, 12, 12, 11, 11, 11, 12, 12, 12, 12, 12, 5, 2, 2, 3, 4, 5};

static PrefixCode *m13_build_meta_code(void)
{
    PrefixCode *pc = prefix_code_alloc();
    if(!pc) return NULL;
    for(int i = 0; i < 37; i++) prefix_code_add_value_low_bit_first(pc, i, meta_code_values[i], meta_code_lengths[i]);
    return pc;
}

static PrefixCode *m13_parse_dynamic_code(int num_codes, PrefixCode *meta, BitStream *bs)
{
    int  length  = 0;
    int *lengths = calloc(num_codes, sizeof(int));
    if(!lengths) return NULL;

    for(int i = 0; i < num_codes; i++)
    {
        int val = prefix_code_read_symbol_le(bs, meta);
        if(val < 0)
        {
            free(lengths);
            return NULL;
        }

        switch(val)
        {
            case 31:
                length = -1;
                break;
            case 32:
                length++;
                break;
            case 33:
                length--;
                break;
            case 34:
                if(bitstream_read_bits_le(bs, 1)) lengths[i++] = length;
                break;
            case 35:
            {
                int rep = bitstream_read_bits_le(bs, 3) + 2;
                while(rep-- > 0) lengths[i++] = length;
                break;
            }
            case 36:
            {
                int rep = bitstream_read_bits_le(bs, 6) + 10;
                while(rep-- > 0) lengths[i++] = length;
                break;
            }
            default:
                length = val + 1;
                break;
        }
        lengths[i] = length;
    }

    int maxlen = 0;
    for(int i = 0; i < num_codes; i++)
        if(lengths[i] > maxlen) maxlen = lengths[i];

    PrefixCode *pc = prefix_code_alloc_with_lengths(lengths, num_codes, maxlen > 0 ? maxlen : 1, true);
    free(lengths);
    return pc;
}

/* Include static code length tables (5 sets) */
#include "method13_tables.h"

int stuffit_method13_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    BitStream bs;
    bitstream_init(&bs, src, src_size);

    /* Read control byte */
    int control = bitstream_read_bits_le(&bs, 8);
    int mode    = control >> 4;

    PrefixCode *first_code = NULL, *second_code = NULL, *offset_code = NULL;

    if(mode == 0)
    {
        PrefixCode *meta = m13_build_meta_code();
        if(!meta) return -1;

        first_code = m13_parse_dynamic_code(321, meta, &bs);
        if(!first_code)
        {
            prefix_code_free(meta);
            return -1;
        }

        if(control & 0x08)
            second_code = first_code; /* shared */
        else
        {
            second_code = m13_parse_dynamic_code(321, meta, &bs);
            if(!second_code)
            {
                prefix_code_free(meta);
                prefix_code_free(first_code);
                return -1;
            }
        }

        offset_code = m13_parse_dynamic_code((control & 0x07) + 10, meta, &bs);
        prefix_code_free(meta);

        if(!offset_code)
        {
            prefix_code_free(first_code);
            if(second_code != first_code) prefix_code_free(second_code);
            return -1;
        }
    }
    else if(mode >= 1 && mode <= 5)
    {
        int idx    = mode - 1;
        int maxlen = 0;
        for(int i = 0; i < 321; i++)
        {
            if(m13_first_code_lengths[idx][i] > maxlen) maxlen = m13_first_code_lengths[idx][i];
        }
        first_code = prefix_code_alloc_with_lengths(m13_first_code_lengths[idx], 321, maxlen, true);

        maxlen = 0;
        for(int i = 0; i < 321; i++)
        {
            if(m13_second_code_lengths[idx][i] > maxlen) maxlen = m13_second_code_lengths[idx][i];
        }
        second_code = prefix_code_alloc_with_lengths(m13_second_code_lengths[idx], 321, maxlen, true);

        maxlen    = 0;
        int osize = m13_offset_code_sizes[idx];
        for(int i = 0; i < osize; i++)
        {
            if(m13_offset_code_lengths[idx][i] > maxlen) maxlen = m13_offset_code_lengths[idx][i];
        }
        offset_code = prefix_code_alloc_with_lengths(m13_offset_code_lengths[idx], osize, maxlen, true);
    }
    else
    {
        *dst_size = 0;
        return -1;
    }

    if(!first_code || !second_code || !offset_code)
    {
        prefix_code_free(first_code);
        if(second_code != first_code) prefix_code_free(second_code);
        prefix_code_free(offset_code);
        return -1;
    }

    uint8_t *window = calloc(1, M13_WINDOW_SIZE);
    if(!window)
    {
        prefix_code_free(first_code);
        if(second_code != first_code) prefix_code_free(second_code);
        prefix_code_free(offset_code);
        return -1;
    }

    int         win_pos = 0;
    PrefixCode *curr    = first_code;

    while(di < limit)
    {
        int sym = prefix_code_read_symbol_le(&bs, curr);
        if(sym < 0) break;

        if(sym < 0x100)
        {
            curr            = first_code;
            dst[di++]       = (uint8_t)sym;
            window[win_pos] = (uint8_t)sym;
            win_pos         = (win_pos + 1) & (M13_WINDOW_SIZE - 1);
        }
        else
        {
            curr = second_code;
            int length, offset;

            if(sym == 0x140) break; /* end */

            if(sym < 0x13e)
                length = sym - 0x100 + 3;
            else if(sym == 0x13e)
                length = bitstream_read_bits_le(&bs, 10) + 65;
            else /* 0x13f */
                length = bitstream_read_bits_le(&bs, 15) + 65;

            int bitlength = prefix_code_read_symbol_le(&bs, offset_code);
            if(bitlength < 0) break;

            if(bitlength == 0)
                offset = 1;
            else if(bitlength == 1)
                offset = 2;
            else
                offset = (1 << (bitlength - 1)) + bitstream_read_bits_le(&bs, bitlength - 1) + 1;

            int src_pos = (win_pos - offset + M13_WINDOW_SIZE) & (M13_WINDOW_SIZE - 1);
            for(int j = 0; j < length && di < limit; j++)
            {
                uint8_t b       = window[src_pos];
                dst[di++]       = b;
                window[win_pos] = b;
                win_pos         = (win_pos + 1) & (M13_WINDOW_SIZE - 1);
                src_pos         = (src_pos + 1) & (M13_WINDOW_SIZE - 1);
            }
        }
    }

    free(window);
    prefix_code_free(first_code);
    if(second_code != first_code) prefix_code_free(second_code);
    prefix_code_free(offset_code);
    *dst_size = di;
    return 0;
}
