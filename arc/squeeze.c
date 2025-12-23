/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2026 Natalia Portillo.
 * Copyright © 2018-2019 David Ryskalczyk
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../library.h"

#define SPEOF   256  // Special end-of-file token.
#define NUMVALS 257  // Number of values in the Huffman tree (256 chars + SPEOF).

// Node structure for the Huffman decoding tree.
struct nd
{
    int child[2];  // Children of the node.
};

// Static variables for the decompression state.
static struct nd nodes[NUMVALS];  // The Huffman tree.
static int       numnodes;        // Number of nodes in the tree.

static int           bpos;   // Bit position in the current byte.
static unsigned char curin;  // Current byte being read.

// Pointers for buffer management.
static const unsigned char *in_buf_ptr;
static size_t               in_len_rem;
static unsigned char       *out_buf_ptr;
static size_t               out_len_rem;

// Reads a byte from the input buffer.
static int get_byte()
{
    if(in_len_rem == 0) { return EOF; }
    in_len_rem--;
    return *in_buf_ptr++;
}

static int arc_decompress_huffman(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf, size_t *out_len)
{
    // Basic validation of pointers.
    if(!in_buf || !out_buf || !out_len) { return -1; }

    // Initialize buffer pointers and lengths.
    in_buf_ptr  = in_buf;
    in_len_rem  = in_len;
    out_buf_ptr = out_buf;
    out_len_rem = *out_len;

    bpos = 99;  // Force initial read.

    // Read the number of nodes in the Huffman tree.
    if(in_len_rem < 2) return -1;
    numnodes = get_byte();
    numnodes |= get_byte() << 8;

    if(numnodes < 0 || numnodes >= NUMVALS)
    {
        return -1;  // Invalid tree.
    }

    // ARC: initialize for possible empty tree (SPEOF only)
    nodes[0].child[0] = -(SPEOF + 1);
    nodes[0].child[1] = -(SPEOF + 1);

    // Read the Huffman tree from the input buffer, sign-extend 16-bit values
    for(int i = 0; i < numnodes; ++i)
    {
        if(in_len_rem < 4) return -1;
        uint8_t b0        = get_byte();
        uint8_t b1        = get_byte();
        uint8_t b2        = get_byte();
        uint8_t b3        = get_byte();
        nodes[i].child[0] = (int16_t)((b0) | (b1 << 8));
        nodes[i].child[1] = (int16_t)((b2) | (b3 << 8));
    }

    size_t written = 0;
    // bpos is already 99 from init

    while(written < *out_len)
    {
        int i = 0;
        // follow bit stream in tree to a leaf
        while(i >= 0)
        {
            if(++bpos > 7)
            {
                int c = get_byte();
                if(c == EOF)
                {
                    *out_len = written;
                    return 0;  // End of input
                }
                curin = c;
                bpos  = 0;
                // move a level deeper in tree
                i     = nodes[i].child[curin & 1];
            }
            else { i = nodes[i].child[1 & (curin >>= 1)]; }
        }

        // decode fake node index to original data value
        int value = -(i + 1);

        if(value == SPEOF)
        {
            break;  // End of data
        }

        *out_buf_ptr++ = value;
        written++;
    }

    *out_len = written;
    return 0;
}

// Decompresses data using Huffman squeezing.
AARU_EXPORT int AARU_CALL arc_decompress_squeeze(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                 size_t *out_len)
{
    size_t         temp_len = *out_len * 2;
    unsigned char *temp_buf = malloc(temp_len);
    if(!temp_buf) return -1;

    int result = arc_decompress_huffman(in_buf, in_len, temp_buf, &temp_len);
    if(result == 0) { result = arc_decompress_pack(temp_buf, temp_len, out_buf, out_len); }

    free(temp_buf);
    return result;
}
