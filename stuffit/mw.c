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

/* StuffIt method 8: MW (dictionary-based LZW variant) */

#include <stdlib.h>
#include "stuffit.h"
#include "stuffit_internal.h"

#define MW_DICT_SIZE  16385
#define MW_STACK_SIZE 16384

int stuffit_mw_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    uint16_t *dict  = calloc(MW_DICT_SIZE, sizeof(uint16_t));
    uint16_t *stack = malloc(MW_STACK_SIZE * sizeof(uint16_t));
    if(!dict || !stack)
    {
        free(dict);
        free(stack);
        return -1;
    }

    StuffitBitReaderLE br;
    stuffit_bits_le_init(&br, src, src_size);

    while(di < limit)
    {
        int max  = 256;
        int max1 = max << 1;
        int bits = 9;
        int ptr  = stuffit_bits_le_read(&br, bits);

        if(ptr >= max) break;

        dict[255] = ptr;

        /* Output initial symbol */
        {
            int sp = 0;
            int p  = ptr;
            while(p >= 256 && sp < MW_STACK_SIZE)
            {
                stack[sp++] = dict[p];
                p           = dict[p - 1];
            }
            if(di < limit) dst[di++] = (uint8_t)p;
            for(int i = sp - 1; i >= 0 && di < limit; i--)
            {
                int s = stack[i];
                while(s >= 256 && sp < MW_STACK_SIZE)
                {
                    stack[sp++] = dict[s];
                    s           = dict[s - 1];
                }
                if(di < limit) dst[di++] = (uint8_t)s;
            }
        }

        while(di < limit)
        {
            ptr = stuffit_bits_le_read(&br, bits);
            if(br.byte_pos > br.data_len + 2) goto done;

            if(ptr >= max) break;

            dict[max++] = ptr;
            if(max == max1)
            {
                max1 <<= 1;
                bits++;
                if(bits > 13) break;
            }

            /* Output decoded string */
            int sp   = 0;
            int p    = ptr;
            stack[0] = p;
            sp       = 1;

            while(sp > 0)
            {
                p = stack[--sp];
                while(p >= 256)
                {
                    if(sp >= MW_STACK_SIZE) goto done;
                    stack[sp++] = dict[p];
                    p           = dict[p - 1];
                }
                if(di < limit) dst[di++] = (uint8_t)p;
            }
        }

        if(ptr > max) break;
    }

done:
    free(dict);
    free(stack);
    *dst_size = di;
    return 0;
}
