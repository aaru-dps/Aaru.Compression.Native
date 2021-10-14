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
#include <stdint.h>

#include "library.h"
#include "apple_rle.h"

// readonly Stream _inStream;
static int32_t count;
static bool    nextA; // true if A, false if B
static uint8_t repeatedByteA, repeatedByteB;
static bool    repeatMode; // true if we're repeating, false if we're just copying

/// <summary>Initializes a decompressor for the specified stream</summary>
/// <param name="stream">Stream containing the compressed data</param>
AARU_EXPORT void AARU_CALL apple_rle_reset()
{
    repeatedByteA = repeatedByteB = 0;
    count                         = 0;
    nextA                         = true;
    repeatMode                    = false;
}

/// <summary>Decompresses a byte</summary>
/// <returns>Decompressed byte</returns>
AARU_EXPORT int32_t AARU_CALL apple_rle_produce_byte(const uint8_t* buffer, int32_t length, int32_t* position)
{
    if(repeatMode && count > 0)
    {
        count--;

        if(nextA)
        {
            nextA = false;

            return repeatedByteA;
        }

        nextA = true;

        return repeatedByteB;
    }

    if(!repeatMode && count > 0)
    {
        count--;

        return buffer[(*position)++];
    }

    if(*position == length) return -1;

    while(true)
    {
        uint8_t b1 = buffer[(*position)++];
        uint8_t b2 = buffer[(*position)++];
        int16_t s  = (int16_t)((b1 << 8) | b2);

        if(s == 0 || s >= DART_CHUNK || s <= -DART_CHUNK) continue;

        if(s < 0)
        {
            repeatMode    = true;
            repeatedByteA = buffer[(*position)++];
            repeatedByteB = buffer[(*position)++];
            count         = (-s * 2) - 1;
            nextA         = false;

            return repeatedByteA;
        }

        if(s <= 0) continue;

        repeatMode = false;
        count      = (s * 2) - 1;

        return buffer[(*position)++];
    }
}