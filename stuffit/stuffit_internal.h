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

#ifndef AARU_STUFFIT_INTERNAL_H
#define AARU_STUFFIT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* MSB-first bitstream reader for StuffIt algorithms */
typedef struct StuffitBitReader
{
    const uint8_t *data;
    size_t         data_len;
    size_t         byte_pos;
    int            bits_left;
    uint32_t       buffer;
} StuffitBitReader;

static inline void stuffit_bits_init(StuffitBitReader *br, const uint8_t *data, size_t data_len)
{
    br->data      = data;
    br->data_len  = data_len;
    br->byte_pos  = 0;
    br->bits_left = 0;
    br->buffer    = 0;
}

static inline int stuffit_bits_read(StuffitBitReader *br, int count)
{
    while(br->bits_left < count)
    {
        uint8_t byte = (br->byte_pos < br->data_len) ? br->data[br->byte_pos++] : 0;
        br->buffer   = (br->buffer << 8) | byte;
        br->bits_left += 8;
    }
    br->bits_left -= count;
    return (br->buffer >> br->bits_left) & ((1 << count) - 1);
}

static inline int stuffit_bits_read_bit(StuffitBitReader *br) { return stuffit_bits_read(br, 1); }

static inline uint8_t stuffit_bits_read_byte(StuffitBitReader *br) { return stuffit_bits_read(br, 8); }

static inline void stuffit_bits_skip_to_boundary(StuffitBitReader *br)
{
    br->bits_left = 0;
    br->buffer    = 0;
}

/* LSB-first bitstream reader */
typedef struct StuffitBitReaderLE
{
    const uint8_t *data;
    size_t         data_len;
    size_t         byte_pos;
    int            bits_left;
    uint32_t       buffer;
} StuffitBitReaderLE;

static inline void stuffit_bits_le_init(StuffitBitReaderLE *br, const uint8_t *data, size_t data_len)
{
    br->data      = data;
    br->data_len  = data_len;
    br->byte_pos  = 0;
    br->bits_left = 0;
    br->buffer    = 0;
}

static inline int stuffit_bits_le_read(StuffitBitReaderLE *br, int count)
{
    while(br->bits_left < count)
    {
        uint8_t byte = (br->byte_pos < br->data_len) ? br->data[br->byte_pos++] : 0;
        br->buffer |= (uint32_t)byte << br->bits_left;
        br->bits_left += 8;
    }
    int val = br->buffer & ((1 << count) - 1);
    br->buffer >>= count;
    br->bits_left -= count;
    return val;
}

static inline int stuffit_bits_le_read_bit(StuffitBitReaderLE *br) { return stuffit_bits_le_read(br, 1); }

static inline uint8_t stuffit_bits_le_read_byte(StuffitBitReaderLE *br) { return stuffit_bits_le_read(br, 8); }

static inline void stuffit_bits_le_skip(StuffitBitReaderLE *br, int count)
{
    for(int i = 0; i < count; i++) stuffit_bits_le_read_bit(br);
}

/* P2 variable-length integer (used by StuffIt X Iron) */
static inline uint64_t stuffit_read_p2(StuffitBitReaderLE *br)
{
    int n = 1;
    while(stuffit_bits_le_read_bit(br) == 1 && n < 64) n++;

    uint64_t value = 0;
    uint64_t bit   = 1;

    while(n)
    {
        if(stuffit_bits_le_read_bit(br) == 1)
        {
            n--;
            value |= bit;
        }
        bit <<= 1;
    }
    return value - 1;
}

/* Reading big-endian integers from raw bytes */
static inline uint32_t stuffit_read_be32(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static inline void stuffit_write_le32(uint8_t *p, int32_t val)
{
    p[0] = val & 0xff;
    p[1] = (val >> 8) & 0xff;
    p[2] = (val >> 16) & 0xff;
    p[3] = (val >> 24) & 0xff;
}

static inline int32_t stuffit_read_le32(const uint8_t *p)
{ return (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16) | ((int32_t)p[3] << 24); }

#endif /* AARU_STUFFIT_INTERNAL_H */
