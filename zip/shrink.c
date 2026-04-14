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

#include "shrink.h"

#include <stdlib.h>
#include <string.h>

#include "../pak/bitstream.h"

/* LZW constants for ZIP Shrink */
#define SHRINK_MAX_BITS   13
#define SHRINK_INIT_BITS  9
#define SHRINK_MAX_CODES  8192 /* 2^13 */
#define SHRINK_CONTROL    256  /* Control code */
#define SHRINK_FIRST_CODE 257  /* First user code */
#define SHRINK_GROW_CODE  1    /* Increment code size */
#define SHRINK_CLEAR_CODE 2    /* Partial clear */

/* LZW dictionary entry */
typedef struct
{
    int16_t parent; /* Parent code, -1 for root entries */
    uint8_t chr;    /* Character at this node */
    uint8_t used;   /* Whether this entry is in use */
} ShrinkEntry;

/* Reverse-walk the dictionary chain and write to a stack buffer, return length */
static int shrink_decode_string(ShrinkEntry *dict, int code, uint8_t *stack, int max_stack)
{
    int count = 0;

    while(code >= 0 && count < max_stack)
    {
        stack[count++] = dict[code].chr;
        code           = dict[code].parent;
    }

    return count;
}

/* Partial clear: mark entries whose parent chains include non-root entries
   that are not themselves parents of other entries */
static void shrink_partial_clear(ShrinkEntry *dict, int num_codes)
{
    /* Mark entries that are referenced as parents */
    uint8_t *is_parent = (uint8_t *)calloc(SHRINK_MAX_CODES, 1);
    if(!is_parent) return;

    for(int i = SHRINK_FIRST_CODE; i < num_codes; i++)
    {
        if(dict[i].used && dict[i].parent >= SHRINK_FIRST_CODE) is_parent[dict[i].parent] = 1;
    }

    /* Clear entries that are not parents of other entries */
    for(int i = SHRINK_FIRST_CODE; i < num_codes; i++)
    {
        if(dict[i].used && !is_parent[i]) dict[i].used = 0;
    }

    free(is_parent);
}

int zip_shrink_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    BitStream    bs;
    ShrinkEntry *dict;
    uint8_t      stack[SHRINK_MAX_CODES];
    size_t       out_pos  = 0;
    size_t       out_size = *out_len;
    int          code_size;
    int          next_code;
    int          prev_code;
    int          code;
    int          len;

    if(!in_buf || !out_buf || !out_len) return -1;

    /* Allocate dictionary */
    dict = (ShrinkEntry *)calloc(SHRINK_MAX_CODES, sizeof(ShrinkEntry));
    if(!dict) return -1;

    /* Initialize dictionary with single-character entries */
    for(int i = 0; i < 256; i++)
    {
        dict[i].parent = -1;
        dict[i].chr    = (uint8_t)i;
        dict[i].used   = 1;
    }

    /* Code 256 is the control code, mark as used */
    dict[SHRINK_CONTROL].parent = -1;
    dict[SHRINK_CONTROL].chr    = 0;
    dict[SHRINK_CONTROL].used   = 1;

    next_code = SHRINK_FIRST_CODE;
    code_size = SHRINK_INIT_BITS;

    bitstream_init(&bs, in_buf, in_len);

    /* Read first code */
    prev_code = (int)bitstream_read_bits_le(&bs, code_size);

    if(prev_code < 256 && out_pos < out_size) out_buf[out_pos++] = (uint8_t)prev_code;

    while(out_pos < out_size && !bitstream_eof(&bs))
    {
        code = (int)bitstream_read_bits_le(&bs, code_size);

        if(bitstream_eof(&bs)) break;

        /* Handle control code */
        if(code == SHRINK_CONTROL)
        {
            int subcode = (int)bitstream_read_bits_le(&bs, code_size);

            if(bitstream_eof(&bs)) break;

            if(subcode == SHRINK_GROW_CODE)
            {
                code_size++;
                if(code_size > SHRINK_MAX_BITS)
                {
                    free(dict);
                    return -1;
                }
            }
            else if(subcode == SHRINK_CLEAR_CODE)
            {
                shrink_partial_clear(dict, next_code);

                /* Reset next_code to search from the beginning for freed entries */
                next_code = SHRINK_FIRST_CODE;
                while(next_code < SHRINK_MAX_CODES && dict[next_code].used) next_code++;
            }

            continue;
        }

        /* Handle KwKwK case: code == next available and not yet in dictionary */
        if(code >= SHRINK_FIRST_CODE && !dict[code].used)
        {
            /* The code is the next one to be added: reconstruct prev_code string + first char */
            len = shrink_decode_string(dict, prev_code, stack, SHRINK_MAX_CODES);

            if(len <= 0)
            {
                free(dict);
                return -1;
            }

            /* First char of previous string is stack[len-1] (stack is in reverse order) */
            uint8_t first_char = stack[len - 1];

            /* Output in forward order: stack is reversed */
            for(int i = len - 1; i >= 0; i--)
            {
                if(out_pos < out_size) out_buf[out_pos++] = stack[i];
            }

            if(out_pos < out_size) out_buf[out_pos++] = first_char;

            /* Add new dictionary entry */
            if(next_code < SHRINK_MAX_CODES)
            {
                dict[next_code].parent = (int16_t)prev_code;
                dict[next_code].chr    = first_char;
                dict[next_code].used   = 1;

                /* Find next free slot */
                next_code++;
                while(next_code < SHRINK_MAX_CODES && dict[next_code].used) next_code++;
            }

            prev_code = code;
            continue;
        }

        /* Normal case: decode the string */
        len = shrink_decode_string(dict, code, stack, SHRINK_MAX_CODES);

        if(len <= 0)
        {
            free(dict);
            return -1;
        }

        /* Output in forward order (stack is reversed) */
        for(int i = len - 1; i >= 0; i--)
        {
            if(out_pos < out_size) out_buf[out_pos++] = stack[i];
        }

        /* Add new dictionary entry: prev_code + first char of current string */
        if(next_code < SHRINK_MAX_CODES)
        {
            dict[next_code].parent = (int16_t)prev_code;
            dict[next_code].chr    = stack[len - 1]; /* first char of decoded string */
            dict[next_code].used   = 1;

            /* Find next free slot (partial clear may have freed interior slots) */
            next_code++;
            while(next_code < SHRINK_MAX_CODES && dict[next_code].used) next_code++;
        }

        prev_code = code;
    }

    free(dict);
    *out_len = out_pos;
    return 0;
}
