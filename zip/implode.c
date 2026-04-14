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

#include "implode.h"

#include <stdlib.h>
#include <string.h>

#include "../pak/bitstream.h"
#include "../pak/prefixcode.h"

/*
 * Parse a Shannon-Fano tree from the ZIP Implode bitstream.
 *
 * Format: 1 byte = number of groups - 1.
 * Each group: 1 byte where high nibble = (count - 1), low nibble = (code length - 1).
 * Codes are assigned from highest value down (shortest code = all ones).
 */
static PrefixCode *implode_read_tree(BitStream *bs, int num_symbols)
{
    int lengths[256];
    int num_groups;
    int symbol;
    int max_length;

    memset(lengths, 0, sizeof(lengths));

    num_groups = (int)bitstream_read_bits_le(bs, 8) + 1;

    symbol     = 0;
    max_length = 0;

    for(int g = 0; g < num_groups; g++)
    {
        int group_byte = (int)bitstream_read_bits_le(bs, 8);
        int count      = ((group_byte >> 4) & 0x0F) + 1;
        int length     = (group_byte & 0x0F) + 1;

        for(int i = 0; i < count && symbol < num_symbols; i++)
        {
            lengths[symbol++] = length;

            if(length > max_length) max_length = length;
        }
    }

    /* Build prefix code tree. shortestCodeIsZeros=false means codes assigned from highest value down. */
    return prefix_code_alloc_with_lengths(lengths, symbol, max_length, false);
}

int zip_implode_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len,
                           int large_dictionary, int has_literals)
{
    BitStream   bs;
    PrefixCode *literal_code  = NULL;
    PrefixCode *length_code   = NULL;
    PrefixCode *distance_code = NULL;
    int         offset_bits; /* Number of low offset bits read raw */
    size_t      out_pos  = 0;
    size_t      out_size = *out_len;
    int         result   = 0;

    if(!in_buf || !out_buf || !out_len) return -1;

    offset_bits = large_dictionary ? 7 : 6;

    bitstream_init(&bs, in_buf, in_len);

    /* Read trees */
    if(has_literals)
    {
        literal_code = implode_read_tree(&bs, 256);

        if(!literal_code)
        {
            result = -1;
            goto cleanup;
        }
    }

    length_code = implode_read_tree(&bs, 64);

    if(!length_code)
    {
        result = -1;
        goto cleanup;
    }

    distance_code = implode_read_tree(&bs, 64);

    if(!distance_code)
    {
        result = -1;
        goto cleanup;
    }

    /* Decompress */
    while(out_pos < out_size && !bitstream_eof(&bs))
    {
        uint32_t flag = bitstream_read_bits_le(&bs, 1);

        if(flag) /* Literal */
        {
            uint8_t byte;

            if(has_literals)
            {
                int sym = prefix_code_read_symbol_le(&bs, literal_code);

                if(sym < 0)
                {
                    result = -1;
                    goto cleanup;
                }

                byte = (uint8_t)sym;
            }
            else
            {
                byte = (uint8_t)bitstream_read_bits_le(&bs, 8);
            }

            out_buf[out_pos++] = byte;
        }
        else /* Match */
        {
            /* Read low offset bits raw */
            uint32_t low_offset = bitstream_read_bits_le(&bs, offset_bits);

            /* Read high offset bits from distance tree */
            int high_offset = prefix_code_read_symbol_le(&bs, distance_code);

            if(high_offset < 0)
            {
                result = -1;
                goto cleanup;
            }

            size_t offset = ((size_t)high_offset << offset_bits) | low_offset;
            offset += 1; /* Offset is 1-based */

            /* Read length from tree */
            int length = prefix_code_read_symbol_le(&bs, length_code);

            if(length < 0)
            {
                result = -1;
                goto cleanup;
            }

            length += 2; /* Minimum match length is 2 */

            /* If length value was 63 (max), read additional byte */
            if(length == 65)
            {
                int extra = (int)bitstream_read_bits_le(&bs, 8);
                length += extra;
            }

            /* If literal tree present, add 1 to length */
            if(has_literals) length++;

            /* Copy from history */
            for(int i = 0; i < length && out_pos < out_size; i++)
            {
                if(offset > out_pos)
                    out_buf[out_pos] = 0;
                else
                    out_buf[out_pos] = out_buf[out_pos - offset];

                out_pos++;
            }
        }
    }

    *out_len = out_pos;

cleanup:
    if(literal_code) prefix_code_free(literal_code);
    if(length_code) prefix_code_free(length_code);
    if(distance_code) prefix_code_free(distance_code);

    return result;
}
