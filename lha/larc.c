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

#include "larc.h"
#include "bitio.h"
#include "lzss.h"

#include <string.h>

/*
 * LArc -lzs- decompression.
 * Window: 2048 bytes.
 * Bit stream, MSB first.
 * Flag bit 1 = literal (8 bits), flag bit 0 = match (11-bit offset + 4-bit length).
 */
AARU_EXPORT int AARU_CALL larc_decompress_lzs(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    lha_bitio bitio;
    lha_lzss  lzss;
    size_t    expected;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    expected = *out_len;

    if(!lha_lzss_init(&lzss, 2048, out_buf, expected)) return -1;

    lha_bitio_init(&bitio, in_buf, in_len);

    while(lzss.out_pos < expected && !lha_bitio_at_eof(&bitio))
    {
        if(lha_bitio_next_bit(&bitio))
        {
            /* Literal byte */
            uint8_t literal = (uint8_t)lha_bitio_next_bits(&bitio, 8);
            lha_lzss_emit_literal(&lzss, literal);
        }
        else
        {
            /* Match */
            int raw_offset = (int)lha_bitio_next_bits(&bitio, 11);
            int length     = (int)lha_bitio_next_bits(&bitio, 4) + 2;
            int offset     = (int)lzss.position - raw_offset - 17;

            lha_lzss_emit_match(&lzss, offset, length);
        }
    }

    *out_len = lzss.out_pos;
    lha_lzss_cleanup(&lzss);
    return 0;
}

/*
 * LArc -lz5- decompression.
 * Window: 4096 bytes, pre-filled with specific byte pattern.
 * Flag-byte stream: each flag byte controls 8 items.
 * Flag bit set = literal (1 byte), flag bit clear = match (2 bytes).
 */
AARU_EXPORT int AARU_CALL larc_decompress_lz5(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    lha_lzss lzss;
    size_t   expected;
    size_t   in_pos;
    int      i;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    expected = *out_len;

    if(!lha_lzss_init(&lzss, 4096, out_buf, expected)) return -1;

    /* Pre-fill window with specific pattern (same as lharc -lh1-) */
    for(i = 0; i < 256; i++) memset(&lzss.window[i * 13 + 18], i, 13);

    for(i = 0; i < 256; i++) lzss.window[256 * 13 + 18 + i] = (uint8_t)i;

    for(i = 0; i < 256; i++) lzss.window[256 * 13 + 256 + 18 + i] = (uint8_t)(255 - i);

    memset(&lzss.window[256 * 13 + 512 + 18], 0, 128);
    memset(&lzss.window[256 * 13 + 512 + 128 + 18], ' ', 128 - 18);

    in_pos = 0;

    while(lzss.out_pos < expected && in_pos < in_len)
    {
        uint8_t flags = in_buf[in_pos++];
        int     bit;

        for(bit = 0; bit < 8 && lzss.out_pos < expected && in_pos < in_len; bit++)
        {
            if(flags & (1 << bit))
            {
                /* Literal */
                lha_lzss_emit_literal(&lzss, in_buf[in_pos++]);
            }
            else
            {
                /* Match: 2 bytes */
                uint8_t byte1, byte2;
                int     offset, length;

                if(in_pos + 1 >= in_len) break;

                byte1 = in_buf[in_pos++];
                byte2 = in_buf[in_pos++];

                offset = (int)lzss.position - (int)byte1 - ((byte2 & 0xf0) << 4) - 18;
                length = (byte2 & 0x0f) + 3;

                lha_lzss_emit_match(&lzss, offset, length);
            }
        }
    }

    *out_len = lzss.out_pos;
    lha_lzss_cleanup(&lzss);
    return 0;
}
