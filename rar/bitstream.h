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

#ifndef AARU_COMPRESSION_NATIVE_RAR_BITSTREAM_H
#define AARU_COMPRESSION_NATIVE_RAR_BITSTREAM_H

#include <stddef.h>
#include <stdint.h>

/**
 * MSB-first bitstream reader for RAR decompression.
 *
 * Bits are extracted from the most significant bit first, matching the
 * convention used by all RAR compression formats (v1.5 through v5.0).
 */
typedef struct
{
    const uint8_t *data;
    size_t         len;
    size_t         byte_pos;
    int            bits_left; /* bits remaining in bit_buf (0..32) */
    uint32_t       bit_buf;   /* left-aligned: MSB is the next bit to extract */
} rar_bitstream_t;

/**
 * Initialize a bitstream reader on a memory buffer.
 */
void rar_bs_init(rar_bitstream_t *bs, const uint8_t *data, size_t len);

/**
 * Read up to 25 bits MSB-first and return them right-aligned.
 * Returns 0 on EOF (caller must check position separately if needed).
 */
uint32_t rar_bs_read_bits(rar_bitstream_t *bs, int n);

/**
 * Read a single bit MSB-first. Returns 0 or 1.
 */
static inline int rar_bs_read_bit(rar_bitstream_t *bs) { return (int)rar_bs_read_bits(bs, 1); }

/**
 * Read a single byte (8 bits) from the bitstream.
 */
static inline uint32_t rar_bs_read_byte(rar_bitstream_t *bs) { return rar_bs_read_bits(bs, 8); }

/**
 * Skip to the next byte boundary.
 */
void rar_bs_skip_to_byte_boundary(rar_bitstream_t *bs);

/**
 * Return the current bit offset from the start of the buffer.
 */
static inline size_t rar_bs_bit_offset(const rar_bitstream_t *bs) { return bs->byte_pos * 8 - (size_t)bs->bits_left; }

/**
 * Inline refill for performance-critical paths (Huffman decode).
 * Ensures at least 25 bits are available if the buffer has data.
 */
static inline void rar_bs_refill_inline(rar_bitstream_t *bs)
{
    while(bs->bits_left <= 24 && bs->byte_pos < bs->len)
    {
        bs->bit_buf |= (uint32_t)bs->data[bs->byte_pos] << (24 - bs->bits_left);
        bs->bits_left += 8;
        bs->byte_pos++;
    }
}

#endif /* AARU_COMPRESSION_NATIVE_RAR_BITSTREAM_H */
