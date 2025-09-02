/*
 * arc_crush.c - ARC Crush decompression algorithm
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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../library.h"
#include "bitstream.h"
#include "lzw.h"

int pak_decompress_crush_internal(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf, size_t *out_len)
{
    if(!in_buf || !out_buf || !out_len || in_len == 0) { return -1; }

    BitStream bs;
    bitstream_init(&bs, (const uint8_t *)in_buf, in_len);

    LZW *lzw = lzw_alloc(8192, 1);
    if(!lzw) { return -1; }

    // Initialize state
    int  symbolsize    = 1;
    int  nextsizebump  = 2;
    bool useliteralbit = true;

    int  numrecentstrings = 0;
    int  ringindex        = 0;
    bool stringring[500];
    memset(stringring, 0, sizeof(stringring));

    int     usageindex = 0x101;
    uint8_t usage[8192];
    memset(usage, 0, sizeof(usage));

    int     currbyte = 0;
    uint8_t buffer[8192];
    size_t  outpos     = 0;
    size_t  max_output = *out_len;

    while(!bitstream_eof(&bs) && outpos < max_output)
    {
        if(!currbyte)
        {
            // Read the next symbol. How depends on the mode we are operating in.
            int symbol;
            if(useliteralbit)
            {
                // Use codes prefixed by a bit that selects literal or string codes.
                // Literals are always 8 bits, strings vary.
                if(bitstream_read_bit_le(&bs)) { symbol = bitstream_read_bits_le(&bs, symbolsize) + 256; }
                else { symbol = bitstream_read_bits_le(&bs, 8); }
            }
            else
            {
                // Use same-length codes for both literals and strings.
                // Due to an optimization quirk in the original decruncher,
                // literals have their bits inverted.
                symbol = bitstream_read_bits_le(&bs, symbolsize);
                if(symbol < 0x100) symbol ^= 0xff;
            }

            // Code 0x100 is the EOF code.
            if(symbol == 0x100) { break; }

            // Walk through the LZW tree, and set the usage count of the current
            // string and all its parents to 4. This is not necessary for literals,
            // but we do it anyway for simplicity.
            LZWTreeNode *nodes      = lzw_symbols(lzw);
            int          marksymbol = symbol;
            while(marksymbol >= 0)
            {
                if(marksymbol < 8192) { usage[marksymbol] = 4; }
                marksymbol = nodes[marksymbol].parent;
            }

            // Adjust the count of recent strings versus literals.
            // Use a ring buffer of length 500 as a window to keep track
            // of how many strings have been encountered lately.

            // First, decrease the count if a string leaves the window.
            if(stringring[ringindex]) numrecentstrings--;

            // Then store the current type of symbol in the window, and
            // increase the count if the current symbol is a string.
            if(symbol < 0x100) { stringring[ringindex] = false; }
            else
            {
                stringring[ringindex] = true;
                numrecentstrings++;
            }

            // Move the window forward.
            ringindex = (ringindex + 1) % 500;

            // Check the number of strings. If there have been many literals
            // lately, bit-prefixed codes should be used. If we need to change
            // mode, re-calculate the point where we increase the code length.
            bool manyliterals = numrecentstrings < 375;
            if(manyliterals != useliteralbit)
            {
                useliteralbit = manyliterals;
                nextsizebump  = 1 << symbolsize;
                if(!useliteralbit) nextsizebump -= 0x100;
            }

            // Update the LZW tree.
            if(!lzw_symbol_list_full(lzw))
            {
                // If there is space in the tree, just add a new string as usual.
                if(lzw_next_symbol(lzw, symbol) != LZW_NO_ERROR)
                {
                    lzw_free(lzw);
                    return -1;
                }

                // Set the usage count of the newly created entry.
                int count = lzw_symbol_count(lzw);
                if(count > 0 && count - 1 < 8192) { usage[count - 1] = 2; }
            }
            else
            {
                // If the tree is full, find a less-used symbol, and replace it.
                int minindex = 0, minusage = INT_MAX;
                int index = usageindex;
                do {
                    index++;
                    if(index == 8192) index = 0x101;

                    if(usage[index] < minusage)
                    {
                        minindex = index;
                        minusage = usage[index];
                    }

                    usage[index]--;
                    if(usage[index] == 0) break;
                } while(index != usageindex);

                usageindex = index;

                if(lzw_replace_symbol(lzw, minindex, symbol) != LZW_NO_ERROR)
                {
                    lzw_free(lzw);
                    return -1;
                }

                // Set the usage count of the replaced entry.
                if(minindex < 8192) { usage[minindex] = 2; }
            }

            // Extract the data to output.
            currbyte = lzw_reverse_output_to_buffer(lzw, buffer);

            // Check if we need to increase the code size. The point at which
            // to increase varies depending on the coding mode.
            if(lzw_symbol_count(lzw) - 257 >= nextsizebump)
            {
                symbolsize++;
                nextsizebump = 1 << symbolsize;
                if(!useliteralbit) nextsizebump -= 0x100;
            }
        }

        if(currbyte > 0 && outpos < max_output) { out_buf[outpos++] = (char)buffer[--currbyte]; }
        else if(currbyte == 0)
        {
            // No more bytes in buffer, continue to next symbol
            continue;
        }
        else
        {
            // Output buffer full
            break;
        }
    }

    lzw_free(lzw);
    *out_len = outpos;
    return 0;
}

AARU_EXPORT int AARU_CALL pak_decompress_crush(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                               size_t *out_len)
{
    // Allocate a temporary buffer.
    size_t         temp_len = *out_len * 2;  // Heuristic.
    unsigned char *temp_buf = malloc(temp_len);
    if(!temp_buf) return -1;

    // Decompress crunched data.
    int result = pak_decompress_crush_internal(in_buf, in_len, temp_buf, &temp_len);
    if(result == 0)
    {
        // Decompress non-repeat packing.
        result = arc_decompress_pack(temp_buf, temp_len, out_buf, out_len);
    }

    free(temp_buf);
    return result;
}