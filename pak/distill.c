/*
 * arc_distill.c - ARC Distill decompression algorithm
 *
 * Copyright (c) 2017-present, MacPaw Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../library.h"
#include "bitstream.h"
#include "prefixcode.h"

static const int offset_lengths[0x40] = {
    3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

static const int offset_codes[0x40] = {
    0x00, 0x02, 0x04, 0x0c, 0x01, 0x06, 0x0a, 0x0e, 0x11, 0x16, 0x1a, 0x1e, 0x05, 0x09, 0x0d, 0x15,
    0x19, 0x1d, 0x25, 0x29, 0x2d, 0x35, 0x39, 0x3d, 0x03, 0x07, 0x0b, 0x13, 0x17, 0x1b, 0x23, 0x27,
    0x2b, 0x33, 0x37, 0x3b, 0x43, 0x47, 0x4b, 0x53, 0x57, 0x5b, 0x63, 0x67, 0x6b, 0x73, 0x77, 0x7b,
    0x0f, 0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f, 0x8f, 0x9f, 0xaf, 0xbf, 0xcf, 0xdf, 0xef, 0xff,
};

static void build_code_from_tree(PrefixCode *code, int *tree, int node, int numnodes, int depth)
{
    if(depth > 64)
    {
        // Too deep - error
        return;
    }

    if(node >= numnodes) { prefix_code_make_leaf_with_value(code, node - numnodes); }
    else
    {
        prefix_code_start_zero_branch(code);
        build_code_from_tree(code, tree, tree[node], numnodes, depth + 1);
        prefix_code_start_one_branch(code);
        build_code_from_tree(code, tree, tree[node + 1], numnodes, depth + 1);
        prefix_code_finish_branches(code);
    }
}

AARU_EXPORT int AARU_CALL pak_decompress_distill(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                 size_t *out_len)
{
    if(!in_buf || !out_buf || !out_len || in_len == 0) { return -1; }

    BitStream bs;
    bitstream_init(&bs, (const uint8_t *)in_buf, in_len);

    // Read header information
    int numnodes   = bitstream_read_uint16_le(&bs);
    int codelength = bitstream_read_byte(&bs);

    if(numnodes < 2 || numnodes > 0x274) { return -1; }

    // Read tree nodes
    int *nodes = malloc(numnodes * sizeof(int));
    if(!nodes) { return -1; }

    for(int i = 0; i < numnodes; i++) { nodes[i] = bitstream_read_bits_le(&bs, codelength); }

    // Build main code tree
    PrefixCode *maincode = prefix_code_alloc();
    if(!maincode)
    {
        free(nodes);
        return -1;
    }

    prefix_code_start_building_tree(maincode);
    build_code_from_tree(maincode, nodes, numnodes - 2, numnodes, 0);

    free(nodes);

    // Build offset code tree
    PrefixCode *offsetcode = prefix_code_alloc();
    if(!offsetcode)
    {
        prefix_code_free(maincode);
        return -1;
    }

    for(int i = 0; i < 0x40; i++)
    {
        if(prefix_code_add_value_low_bit_first(offsetcode, i, offset_codes[i], offset_lengths[i]) != PREFIX_CODE_OK)
        {
            prefix_code_free(maincode);
            prefix_code_free(offsetcode);
            return -1;
        }
    }

    // LZSS decompression
    uint8_t window[8192];
    memset(window, 0, sizeof(window));
    int    windowpos  = 0;
    size_t outpos     = 0;
    size_t max_output = *out_len;

    while(!bitstream_eof(&bs) && outpos < max_output)
    {
        int symbol = prefix_code_read_symbol_le(&bs, maincode);
        if(symbol < 0) break;

        if(symbol < 256)
        {
            // Literal byte
            if(outpos < max_output) { out_buf[outpos++] = (char)symbol; }
            window[windowpos] = symbol;
            windowpos         = (windowpos + 1) & 0x1fff;
        }
        else if(symbol == 256)
        {
            // End of stream
            break;
        }
        else
        {
            // Match
            int length       = symbol - 0x101 + 3;
            int offsetsymbol = prefix_code_read_symbol_le(&bs, offsetcode);
            if(offsetsymbol < 0) break;

            int extralength;
            if(outpos >= 0x1000 - 0x3c)
                extralength = 7;
            else if(outpos >= 0x800 - 0x3c)
                extralength = 6;
            else if(outpos >= 0x400 - 0x3c)
                extralength = 5;
            else if(outpos >= 0x200 - 0x3c)
                extralength = 4;
            else if(outpos >= 0x100 - 0x3c)
                extralength = 3;
            else if(outpos >= 0x80 - 0x3c)
                extralength = 2;
            else if(outpos >= 0x40 - 0x3c)
                extralength = 1;
            else
                extralength = 0;

            int extrabits = bitstream_read_bits_le(&bs, extralength);
            int offset    = (offsetsymbol << extralength) + extrabits + 1;

            // Copy match
            for(int i = 0; i < length; i++)
            {
                int     sourcepos = (windowpos - offset) & 0x1fff;
                uint8_t byte      = window[sourcepos];

                if(outpos < max_output) { out_buf[outpos++] = (char)byte; }

                window[windowpos] = byte;
                windowpos         = (windowpos + 1) & 0x1fff;
            }
        }
    }

    prefix_code_free(maincode);
    prefix_code_free(offsetcode);

    *out_len = outpos;
    return 0;
}
