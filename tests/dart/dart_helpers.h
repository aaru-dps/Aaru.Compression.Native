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

#ifndef AARU_TESTS_DART_HELPERS_H
#define AARU_TESTS_DART_HELPERS_H

#include <cstddef>
#include <cstdint>

// DART header constants
#define DART_COMPRESS_RLE  0
#define DART_COMPRESS_LZH  1
#define DART_COMPRESS_NONE 2

#define DART_SECTORS_PER_BLOCK 40
#define DART_SECTOR_SIZE       512
#define DART_TAG_SECTOR_SIZE   12
#define DART_DATA_SIZE         (DART_SECTORS_PER_BLOCK * DART_SECTOR_SIZE)      // 20480
#define DART_TAG_SIZE          (DART_SECTORS_PER_BLOCK * DART_TAG_SECTOR_SIZE)  // 480
#define DART_BUFFER_SIZE       (DART_DATA_SIZE + DART_TAG_SIZE)                 // 20960

#define DART_BLOCK_ARRAY_LEN_LOW  40
#define DART_BLOCK_ARRAY_LEN_HIGH 72

#define DART_HEADER_SIZE 4

// DART disk types
#define DART_TYPE_MAC    1
#define DART_TYPE_LISA   2
#define DART_TYPE_APPLE2 3
#define DART_TYPE_MAC_HD 16
#define DART_TYPE_DOS    17
#define DART_TYPE_DOS_HD 18

#pragma pack(push, 1)

typedef struct
{
    uint8_t  srcCmp;   // Compression type (0=RLE, 1=LZH, 2=None)
    uint8_t  srcType;  // Disk type
    uint16_t srcSize;  // Size in blocks (big-endian)
} dart_header_t;

#pragma pack(pop)

static inline uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static inline int16_t be16s(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

// Returns the number of block length entries based on disk type
static inline int dart_block_array_len(uint8_t srcType)
{
    switch(srcType)
    {
        case DART_TYPE_MAC_HD:
        case DART_TYPE_DOS_HD:
            return DART_BLOCK_ARRAY_LEN_HIGH;
        default:
            return DART_BLOCK_ARRAY_LEN_LOW;
    }
}

// Parse DART header from raw data. Returns the offset past the block lengths array.
static inline size_t dart_parse_header(const uint8_t *data, size_t data_len, dart_header_t *hdr, int16_t *block_lengths,
                                       int *num_blocks)
{
    if(data_len < DART_HEADER_SIZE) return 0;

    hdr->srcCmp  = data[0];
    hdr->srcType = data[1];
    hdr->srcSize = be16(data + 2);

    *num_blocks   = dart_block_array_len(hdr->srcType);
    size_t needed = DART_HEADER_SIZE + (*num_blocks) * 2;

    if(data_len < needed) return 0;

    const uint8_t *blk_ptr = data + DART_HEADER_SIZE;
    for(int i = 0; i < *num_blocks; i++) { block_lengths[i] = be16s(blk_ptr + i * 2); }

    return needed;
}

#endif  // AARU_TESTS_DART_HELPERS_H
