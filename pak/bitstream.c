/*
 * bitstream.c - Bit stream input implementation
 *
 * Copyright (c) 2017-present, MacPaw Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */

#include "bitstream.h"

void bitstream_init(BitStream *bs, const uint8_t *data, size_t length)
{
    bs->data      = data;
    bs->length    = length;
    bs->pos       = 0;
    bs->bitbuffer = 0;
    bs->bitcount  = 0;
    bs->eof       = false;
}

static void bitstream_fill_buffer(BitStream *bs)
{
    while(bs->bitcount < 24 && bs->pos < bs->length)
    {
        bs->bitbuffer |= (uint32_t)bs->data[bs->pos] << (24 - bs->bitcount);
        bs->bitcount += 8;
        bs->pos++;
    }
    if(bs->pos >= bs->length && bs->bitcount == 0) { bs->eof = true; }
}

uint32_t bitstream_read_bit(BitStream *bs)
{
    if(bs->eof) return 0;

    if(bs->bitcount == 0) { bitstream_fill_buffer(bs); }

    if(bs->bitcount == 0)
    {
        bs->eof = true;
        return 0;
    }

    uint32_t bit = (bs->bitbuffer >> 31) & 1;
    bs->bitbuffer <<= 1;
    bs->bitcount--;

    return bit;
}

uint32_t bitstream_read_bit_le(BitStream *bs)
{
    if(bs->eof) return 0;

    if(bs->bitcount == 0)
    {
        if(bs->pos >= bs->length)
        {
            bs->eof = true;
            return 0;
        }
        bs->bitbuffer = bs->data[bs->pos++];
        bs->bitcount  = 8;
    }

    uint32_t bit = bs->bitbuffer & 1;
    bs->bitbuffer >>= 1;
    bs->bitcount--;

    return bit;
}

uint32_t bitstream_read_bits(BitStream *bs, int count)
{
    uint32_t result = 0;
    for(int i = 0; i < count; i++) { result = (result << 1) | bitstream_read_bit(bs); }
    return result;
}

uint32_t bitstream_read_bits_le(BitStream *bs, int count)
{
    uint32_t result = 0;
    for(int i = 0; i < count; i++) { result |= bitstream_read_bit_le(bs) << i; }
    return result;
}

uint32_t bitstream_peek_bits(BitStream *bs, int count)
{
    // Save current state
    uint32_t saved_buffer   = bs->bitbuffer;
    int      saved_bitcount = bs->bitcount;
    size_t   saved_pos      = bs->pos;
    bool     saved_eof      = bs->eof;

    // Read the bits
    uint32_t result = bitstream_read_bits(bs, count);

    // Restore state
    bs->bitbuffer = saved_buffer;
    bs->bitcount  = saved_bitcount;
    bs->pos       = saved_pos;
    bs->eof       = saved_eof;

    return result;
}

uint32_t bitstream_peek_bits_le(BitStream *bs, int count)
{
    // Save current state
    uint32_t saved_buffer   = bs->bitbuffer;
    int      saved_bitcount = bs->bitcount;
    size_t   saved_pos      = bs->pos;
    bool     saved_eof      = bs->eof;

    // Read the bits
    uint32_t result = bitstream_read_bits_le(bs, count);

    // Restore state
    bs->bitbuffer = saved_buffer;
    bs->bitcount  = saved_bitcount;
    bs->pos       = saved_pos;
    bs->eof       = saved_eof;

    return result;
}

void bitstream_skip_bits(BitStream *bs, int count) { bitstream_read_bits(bs, count); }

void bitstream_skip_bits_le(BitStream *bs, int count) { bitstream_read_bits_le(bs, count); }

uint8_t bitstream_read_byte(BitStream *bs)
{
    if(bs->pos >= bs->length)
    {
        bs->eof = true;
        return 0;
    }
    return bs->data[bs->pos++];
}

uint16_t bitstream_read_uint16_le(BitStream *bs)
{
    if(bs->pos + 1 >= bs->length)
    {
        bs->eof = true;
        return 0;
    }
    uint16_t result = bs->data[bs->pos] | (bs->data[bs->pos + 1] << 8);
    bs->pos += 2;
    return result;
}

bool bitstream_eof(BitStream *bs) { return bs->eof; }
