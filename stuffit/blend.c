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

/* StuffIt X method 4: Blend (frame-based multiplexer for Darkhorse/Cyanide/Brimstone) */

#include <stdlib.h>
#include <string.h>
#include "stuffit.h"
#include "stuffit_internal.h"

int stuffitx_blend_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;
    size_t si    = 0;

    while(di < limit && si + 6 <= src_size)
    {
        /* Read 6-byte lookahead for frame detection */
        uint8_t buf[6];
        memcpy(buf, src + si, 6);

        /* Find valid frame header: 0x77 marker + format byte 0-3 */
        for(;;)
        {
            if(si + 6 > src_size) goto done;

            if(buf[0] == 0x77 && buf[1] <= 3)
            {
                /* Check for a possible later match to disambiguate */
                int later = ((buf[2] == 0x77 && buf[3] <= 3) || (buf[3] == 0x77 && buf[4] <= 3) ||
                             (buf[4] == 0x77 && buf[5] <= 3));

                if(!later) break;
                if((stuffit_read_be32(&buf[2]) & 0x1fff) == 0) break;
            }

            memmove(buf, buf + 1, 5);
            si++;
            if(si + 5 >= src_size) goto done;
            buf[5] = src[si + 5];
        }

        int      format   = buf[1];
        uint32_t out_size = stuffit_read_be32(&buf[2]);
        si += 6; /* skip marker + format + 4-byte size */

        /* Remaining compressed data from this point */
        const uint8_t *frame_data  = src + si;
        size_t         frame_avail = src_size - si;

        if(format == 0)
        {
            /* Direct copy */
            size_t copy = out_size;
            if(copy > frame_avail) copy = frame_avail;
            if(di + copy > limit) copy = limit - di;
            memcpy(dst + di, frame_data, copy);
            di += copy;
            si += copy;
        }
        else if(format == 1)
        {
            /* Darkhorse: 1 byte window_bits, then compressed data */
            if(frame_avail < 1) break;
            int window_bits = frame_data[0];

            size_t dec_size = out_size;
            if(di + dec_size > limit) dec_size = limit - di;

            int err =
                stuffitx_darkhorse_decode_buffer(dst + di, &dec_size, frame_data + 1, frame_avail - 1, window_bits);
            if(err < 0) return err;
            di += dec_size;
            si = src_size; /* consumed to end (no way to know exact amount) */
        }
        else if(format == 2)
        {
            /* Cyanide */
            size_t dec_size = out_size;
            if(di + dec_size > limit) dec_size = limit - di;

            int err = stuffitx_cyanide_decode_buffer(dst + di, &dec_size, frame_data, frame_avail);
            if(err < 0) return err;
            di += dec_size;
            si = src_size;
        }
        else if(format == 3)
        {
            /* Brimstone: 1 byte alloc_exp, 1 byte order, then compressed data */
            if(frame_avail < 2) break;
            int alloc_exp  = frame_data[0];
            int order      = frame_data[1];
            int alloc_size = 1 << alloc_exp;

            size_t dec_size = out_size;
            if(di + dec_size > limit) dec_size = limit - di;

            int err = stuffitx_brimstone_decode_buffer(dst + di, &dec_size, frame_data + 2, frame_avail - 2, order,
                                                       alloc_size);
            if(err < 0) return err;
            di += dec_size;
            si = src_size; /* consumed to end */
        }
    }

done:
    *dst_size = di;
    return 0;
}
