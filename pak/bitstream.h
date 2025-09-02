/*
 * bitstream.h - Bit stream input implementation
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

#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BitStream
{
    const uint8_t *data;
    size_t         length;
    size_t         pos;
    uint32_t       bitbuffer;
    int            bitcount;
    bool           eof;
} BitStream;

// Initialize bit stream
void bitstream_init(BitStream *bs, const uint8_t *data, size_t length);

// Read a single bit (MSB first)
uint32_t bitstream_read_bit(BitStream *bs);

// Read a single bit (LSB first)
uint32_t bitstream_read_bit_le(BitStream *bs);

// Read multiple bits (MSB first)
uint32_t bitstream_read_bits(BitStream *bs, int count);

// Read multiple bits (LSB first)
uint32_t bitstream_read_bits_le(BitStream *bs, int count);

// Peek at bits without consuming them (MSB first)
uint32_t bitstream_peek_bits(BitStream *bs, int count);

// Peek at bits without consuming them (LSB first)
uint32_t bitstream_peek_bits_le(BitStream *bs, int count);

// Skip previously peeked bits (MSB first)
void bitstream_skip_bits(BitStream *bs, int count);

// Skip previously peeked bits (LSB first)
void bitstream_skip_bits_le(BitStream *bs, int count);

// Read a byte
uint8_t bitstream_read_byte(BitStream *bs);

// Read a 16-bit little endian integer
uint16_t bitstream_read_uint16_le(BitStream *bs);

// Check if end of stream reached
bool bitstream_eof(BitStream *bs);

#endif /* BITSTREAM_H */
