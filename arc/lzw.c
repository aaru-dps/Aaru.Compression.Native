/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2025 Natalia Portillo.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../library.h"

#define CRBITS     12                // Max bits for crunching.
#define SQBITS     13                // Max bits for squashing.
#define INIT_BITS  9                 // Initial number of bits per code.
#define MAXCODE(n) ((1 << (n)) - 1)  // Macro to calculate max code for n bits.
#define FIRST      257               // First available code.
#define CLEAR      256               // Code to clear the dictionary.

// LZW decompression state variables.
static int             Bits;
static int             max_maxcode;
static int             n_bits;
static int             maxcode;
static int             clear_flg;
static int             free_ent;
static unsigned short *prefix;
static unsigned char  *suffix;
static unsigned char  *stack;

// Buffer management variables.
static const unsigned char *in_buf_ptr;
static size_t               in_len_rem;
static int                  offset;
static char                 buf[SQBITS];

// Reads a variable-length code from the input buffer.
static int getcode()
{
    int            code;
    static int     size = 0;
    int            r_off, bits;
    unsigned char *bp = (unsigned char *)buf;

    // Check if we need to increase code size or handle a clear flag.
    if(clear_flg > 0 || offset >= size || free_ent > maxcode)
    {
        if(free_ent > maxcode)
        {
            n_bits++;
            if(n_bits == Bits)
                maxcode = max_maxcode;
            else
                maxcode = MAXCODE(n_bits);
        }
        if(clear_flg > 0)
        {
            maxcode   = MAXCODE(n_bits = INIT_BITS);
            clear_flg = 0;
        }
        // Read n_bits bytes into the buffer.
        for(size = 0; size < n_bits; size++)
        {
            if(in_len_rem == 0)
            {
                code = -1;
                break;
            }
            code = *in_buf_ptr++;
            in_len_rem--;
            buf[size] = (char)code;
        }
        if(size <= 0) return -1;  // End of file.

        offset = 0;
        size   = (size << 3) - (n_bits - 1);
    }
    r_off = offset;
    bits  = n_bits;

    // Extract the code from the buffer.
    bp += (r_off >> 3);
    r_off &= 7;

    code = (*bp++ >> r_off);
    bits -= 8 - r_off;
    r_off = 8 - r_off;

    if(bits >= 8)
    {
        code |= *bp++ << r_off;
        r_off += 8;
        bits -= 8;
    }
    code |= (*bp & ((1 << bits) - 1)) << r_off;
    offset += n_bits;

    return code;
}

// Main LZW decompression logic.
static int arc_decompress_lzw(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf, size_t *out_len,
                              int squash)
{
    // Basic validation of pointers.
    if(!in_buf || !out_buf || !out_len) { return -1; }

    // Initialize buffer pointers and lengths.
    in_buf_ptr = in_buf;
    in_len_rem = in_len;

    // Set parameters based on whether we're unsquashing or uncrushing.
    if(squash) { Bits = SQBITS; }
    else
    {
        Bits = CRBITS;
        if(in_len_rem > 0)
        {
            // Crunch format has a header byte indicating max bits.
            if(*in_buf_ptr != CRBITS) return -1;
            in_buf_ptr++;
            in_len_rem--;
        }
    }

    if(in_len_rem <= 0)
    {
        *out_len = 0;
        return 0;
    }

    // Initialize LZW parameters.
    max_maxcode = 1 << Bits;
    clear_flg   = 0;
    n_bits      = INIT_BITS;
    maxcode     = MAXCODE(n_bits);

    // Allocate memory for LZW tables.
    prefix = (unsigned short *)malloc(max_maxcode * sizeof(unsigned short));
    suffix = (unsigned char *)malloc(max_maxcode * sizeof(unsigned char));
    stack  = (unsigned char *)malloc(max_maxcode * sizeof(unsigned char));

    if(!prefix || !suffix || !stack)
    {
        if(prefix) free(prefix);
        if(suffix) free(suffix);
        if(stack) free(stack);
        return -1;
    }

    // Initialize the first 256 entries of the dictionary.
    memset(prefix, 0, 256 * sizeof(unsigned short));
    for(int code = 255; code >= 0; code--) { suffix[code] = (unsigned char)code; }

    free_ent = FIRST;
    offset   = 0;

    // Main decompression loop.
    int finchar, oldcode, incode;
    finchar = oldcode = getcode();
    if(oldcode == -1)
    {
        *out_len = 0;
        free(prefix);
        free(suffix);
        free(stack);
        return 0;
    }

    size_t out_pos = 0;
    if(out_pos < *out_len) { out_buf[out_pos++] = finchar; }

    unsigned char *stackp = stack;
    int            code;
    while((code = getcode()) > -1)
    {
        if(code == CLEAR)
        {
            // Clear the dictionary.
            memset(prefix, 0, 256 * sizeof(unsigned short));
            clear_flg = 1;
            free_ent  = FIRST - 1;
            if((code = getcode()) == -1) break;
        }
        incode = code;
        // Handle KwKwK case.
        if(code >= free_ent)
        {
            if(code > free_ent)
            {
                // Error: invalid code.
                break;
            }
            *stackp++ = finchar;
            code      = oldcode;
        }
        // Decode the string by traversing the dictionary.
        while(code >= 256)
        {
            *stackp++ = suffix[code];
            code      = prefix[code];
        }
        *stackp++ = finchar = suffix[code];

        // Write the decoded string to the output buffer.
        do {
            if(out_pos < *out_len) { out_buf[out_pos++] = *--stackp; }
            else
            {
                stackp--;  // Discard if output buffer is full.
            }
        } while(stackp > stack);

        // Add the new string to the dictionary.
        if((code = free_ent) < max_maxcode)
        {
            prefix[code] = (unsigned short)oldcode;
            suffix[code] = finchar;
            free_ent     = code + 1;
        }
        oldcode = incode;
    }

    // Clean up and return.
    *out_len = out_pos;
    free(prefix);
    free(suffix);
    free(stack);
    return 0;
}

// Decompresses squashed data.
AARU_EXPORT int AARU_CALL arc_decompress_squash(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                size_t *out_len)
{
    return arc_decompress_lzw(in_buf, in_len, out_buf, out_len, 1);
}

// Decompresses crunched data.
AARU_EXPORT int AARU_CALL arc_decompress_crunch_dynamic(const unsigned char *in_buf, size_t in_len,
                                                        unsigned char *out_buf, size_t *out_len)
{
    // Allocate a temporary buffer.
    size_t         temp_len = *out_len * 2;  // Heuristic.
    unsigned char *temp_buf = malloc(temp_len);
    if(!temp_buf) return -1;

    // Decompress crunched data.
    int result = arc_decompress_lzw(in_buf, in_len, temp_buf, &temp_len, 0);
    if(result == 0)
    {
        // Decompress non-repeat packing.
        result = arc_decompress_pack(temp_buf, temp_len, out_buf, out_len);
    }

    free(temp_buf);
    return result;
}
