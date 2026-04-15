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

#include "bitstream.h"

void rar_bs_init(rar_bitstream_t *bs, const uint8_t *data, size_t len)
{
    bs->data      = data;
    bs->len       = len;
    bs->byte_pos  = 0;
    bs->bits_left = 0;
    bs->bit_buf   = 0;
}

/**
 * Refill the bit buffer to contain at least 25 bits if possible.
 * We keep bits in the top of bit_buf: the next bit to extract is always at bit 31.
 */
static void rar_bs_refill(rar_bitstream_t *bs)
{
    while(bs->bits_left <= 24 && bs->byte_pos < bs->len)
    {
        bs->bit_buf |= (uint32_t)bs->data[bs->byte_pos] << (24 - bs->bits_left);
        bs->bits_left += 8;
        bs->byte_pos++;
    }
}

uint32_t rar_bs_read_bits(rar_bitstream_t *bs, int n)
{
    uint32_t result;

    if(n <= 0) return 0;

    rar_bs_refill(bs);

    /* Extract the top n bits */
    result = bs->bit_buf >> (32 - n);
    bs->bit_buf <<= n;
    bs->bits_left -= n;

    if(bs->bits_left < 0) bs->bits_left = 0;

    return result;
}

void rar_bs_skip_to_byte_boundary(rar_bitstream_t *bs)
{
    int skip = bs->bits_left & 7;
    if(skip > 0)
    {
        bs->bit_buf <<= skip;
        bs->bits_left -= skip;
    }
}
