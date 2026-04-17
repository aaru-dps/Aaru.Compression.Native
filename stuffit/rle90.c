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

/* StuffIt method 1: RLE with 0x90 escape byte */

#include "stuffit.h"

int stuffit_rle90_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t  si    = 0;
    size_t  di    = 0;
    size_t  limit = *dst_size;
    uint8_t last  = 0;

    while(si < src_size && di < limit)
    {
        uint8_t b = src[si++];

        if(b != 0x90)
        {
            last      = b;
            dst[di++] = b;
        }
        else
        {
            if(si >= src_size) return -1;
            uint8_t c = src[si++];

            if(c == 0)
            {
                last      = 0x90;
                dst[di++] = 0x90;
            }
            else if(c == 1) { return -1; /* invalid */ }
            else
            {
                int count = c - 2;
                for(int j = 0; j < count && di < limit; j++) dst[di++] = last;
            }
        }
    }

    *dst_size = di;
    return 0;
}
