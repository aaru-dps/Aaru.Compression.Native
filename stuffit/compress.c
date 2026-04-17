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

/* StuffIt method 2: UNIX compress LZW (flags=0x8E: block mode, 14-bit max) */

#include <stdlib.h>
#include <string.h>
#include "stuffit.h"
#include "stuffit_internal.h"

#define COMPRESS_BLOCKMODE  0x80
#define COMPRESS_MAX_BITS   14
#define COMPRESS_FIRST_CODE 257 /* 256 literals + clear code */
#define COMPRESS_CLEAR_CODE 256

typedef struct CompressEntry
{
    uint16_t prefix;
    uint8_t  byte;
} CompressEntry;

int stuffit_compress_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t             limit = *dst_size;
    size_t             di    = 0;
    StuffitBitReaderLE br;
    stuffit_bits_le_init(&br, src, src_size);

    int max_symbols = 1 << COMPRESS_MAX_BITS;
    int blockmode   = 1; /* flags & 0x80 */

    CompressEntry *dict = calloc(max_symbols, sizeof(CompressEntry));
    if(!dict) return -1;

    uint8_t *stack = malloc(max_symbols);
    if(!stack)
    {
        free(dict);
        return -1;
    }

    /* Initialize dictionary with single-byte entries */
    for(int i = 0; i < 256; i++)
    {
        dict[i].prefix = 0xffff;
        dict[i].byte   = (uint8_t)i;
    }

    int     next_code    = COMPRESS_FIRST_CODE;
    int     bits         = 9;
    int     symbol_count = 0;
    int     prev_code    = -1;
    uint8_t first_byte   = 0;

    while(di < limit)
    {
        int code = stuffit_bits_le_read(&br, bits);
        symbol_count++;

        if(br.byte_pos > br.data_len + 2) break;

        if(code == COMPRESS_CLEAR_CODE && blockmode)
        {
            /* Skip padding bits to byte boundary */
            int symbolsize = bits;
            if(symbol_count % 8) stuffit_bits_le_skip(&br, symbolsize * (8 - symbol_count % 8));

            /* Reset dictionary */
            next_code    = COMPRESS_FIRST_CODE;
            bits         = 9;
            symbol_count = 0;
            prev_code    = -1;
            continue;
        }

        if(code < 0 || code > next_code) break;

        /* Decode string to stack */
        int stack_ptr = 0;
        int c         = code;

        if(c == next_code)
        {
            /* Special case: code not yet in table */
            stack[stack_ptr++] = first_byte;
            c                  = prev_code;
        }

        while(c >= 256 && stack_ptr < max_symbols)
        {
            stack[stack_ptr++] = dict[c].byte;
            c                  = dict[c].prefix;
        }
        stack[stack_ptr++] = (uint8_t)c;
        first_byte         = (uint8_t)c;

        /* Output in reverse order */
        for(int i = stack_ptr - 1; i >= 0 && di < limit; i--) dst[di++] = stack[i];

        /* Add new dictionary entry */
        if(prev_code >= 0 && next_code < max_symbols)
        {
            dict[next_code].prefix = prev_code;
            dict[next_code].byte   = first_byte;
            next_code++;

            if(next_code >= (1 << bits) && bits < COMPRESS_MAX_BITS) bits++;
        }

        prev_code = code;
    }

    free(dict);
    free(stack);
    *dst_size = di;
    return 0;
}
