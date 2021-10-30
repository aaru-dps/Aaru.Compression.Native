/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2021 Natalia Portillo.
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "library.h"
#include "apple_rle.h"

AARU_EXPORT int32_t AARU_CALL apple_rle_decode_buffer(uint8_t*       dst_buffer,
                                                      int32_t        dst_size,
                                                      const uint8_t* src_buffer,
                                                      int32_t        src_size)
{
    static int32_t count         = 0;
    static bool    nextA         = true; // true if A, false if B
    static uint8_t repeatedByteA = 0, repeatedByteB = 0;
    static bool    repeatMode = false; // true if we're repeating, false if we're just copying
    int32_t        in_pos = 0, out_pos = 0;

    while(in_pos <= src_size && out_pos <= dst_size)
    {
        if(repeatMode && count > 0)
        {
            (count)--;

            if(nextA)
            {
                nextA = false;

                dst_buffer[out_pos++] = repeatedByteA;
                continue;
            }

            nextA = true;

            dst_buffer[out_pos++] = repeatedByteB;
            continue;
        }

        if(!repeatMode && count > 0)
        {
            count--;

            dst_buffer[out_pos++] = src_buffer[in_pos++];
            continue;
        }

        if(in_pos == src_size) break;

        while(true)
        {
            uint8_t b1 = src_buffer[in_pos++];
            uint8_t b2 = src_buffer[in_pos++];
            int16_t s  = (int16_t)((b1 << 8) | b2);

            if(s == 0 || s >= DART_CHUNK || s <= -DART_CHUNK) continue;

            if(s < 0)
            {
                repeatMode    = true;
                repeatedByteA = src_buffer[in_pos++];
                repeatedByteB = src_buffer[in_pos++];
                count         = (-s * 2) - 1;
                nextA         = false;

                dst_buffer[out_pos++] = repeatedByteA;
                break;
            }

            repeatMode = false;
            count      = (s * 2) - 1;

            dst_buffer[out_pos++] = src_buffer[in_pos++];
            break;
        }
    }

    return out_pos;
}