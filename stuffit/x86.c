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

/* StuffIt X preprocessor 2: x86 executable address transformation */

#include <string.h>
#include "stuffit.h"
#include "stuffit_internal.h"

int stuffitx_x86_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t   limit    = *dst_size;
    size_t   si       = 0;
    size_t   di       = 0;
    int      lasthit  = -6;
    uint32_t bitfield = 0;

    static const int table[8]  = {1, 1, 1, 0, 1, 0, 0, 0};
    static const int shifts[8] = {24, 16, 8, 8, 0, 0, 0, 0};

    while(si < src_size && di < limit)
    {
        uint8_t b = src[si++];

        if(b == 0xe8 || b == 0xe9)
        {
            int dist = (int)di - lasthit;
            lasthit  = (int)di;

            if(dist > 5)
                bitfield = 0;
            else
                for(int i = 0; i < dist; i++) bitfield = (bitfield & 0x77) << 1;

            dst[di++] = b;

            /* Check if we have 4 bytes to read */
            if(si + 4 > src_size)
            {
                /* Not enough data for offset, just copy remaining */
                while(si < src_size && di < limit) dst[di++] = src[si++];
                break;
            }

            uint8_t offset_buf[4];
            memcpy(offset_buf, src + si, 4);

            if(offset_buf[3] == 0x00 || offset_buf[3] == 0xff)
            {
                if(table[(bitfield >> 1) & 0x07] && (bitfield >> 1) <= 0x0f)
                {
                    int32_t absaddr = stuffit_read_le32(offset_buf);
                    int32_t reladdr;

                    for(;;)
                    {
                        reladdr = absaddr - (int32_t)di - 6 + 1;
                        if(bitfield == 0) break;

                        int shift     = shifts[bitfield >> 1];
                        int something = (reladdr >> shift) & 0xff;
                        if(something != 0 && something != 0xff) break;
                        absaddr = reladdr ^ ((1 << (shift + 8)) - 1);
                    }

                    reladdr &= 0x1ffffff;
                    if(reladdr >= 0x1000000) reladdr |= (int32_t)0xff000000;

                    stuffit_write_le32(offset_buf, reladdr);
                    si += 4;
                    if(di + 4 <= limit)
                    {
                        memcpy(dst + di, offset_buf, 4);
                        di += 4;
                    }
                    bitfield = 0;
                }
                else
                {
                    bitfield |= 0x11;
                }
            }
            else
            {
                bitfield |= 0x01;
            }

            /* If we didn't consume the offset bytes above, they pass through normally */
            if(si < src_size && di < limit && !(offset_buf[3] == 0x00 || offset_buf[3] == 0xff) ||
               !table[(bitfield >> 1) & 0x07])
            {
                /* Already handled or will be handled in next iteration */
            }
        }
        else
        {
            dst[di++] = b;
        }
    }

    *dst_size = di;
    return 0;
}
