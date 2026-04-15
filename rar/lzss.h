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

#ifndef AARU_COMPRESSION_NATIVE_RAR_LZSS_H
#define AARU_COMPRESSION_NATIVE_RAR_LZSS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * Sliding window LZSS buffer for RAR 3.0 and 5.0 decompression.
 *
 * Uses a power-of-2 sized circular window with mask-based wrapping.
 */
typedef struct
{
    uint8_t *window;
    size_t   mask;     /* window_size - 1 */
    int64_t  position; /* total bytes emitted */
} rar_lzss_t;

/**
 * Initialize the LZSS window. window_size must be a power of 2.
 * @return 0 on success, -1 on allocation failure.
 */
int rar_lzss_init(rar_lzss_t *lzss, size_t window_size);

/**
 * Reset position and clear window contents.
 */
void rar_lzss_restart(rar_lzss_t *lzss);

/**
 * Free the LZSS window buffer.
 */
void rar_lzss_cleanup(rar_lzss_t *lzss);

/**
 * Emit a literal byte into the window.
 */
static inline void rar_lzss_emit_literal(rar_lzss_t *lzss, uint8_t byte)
{
    lzss->window[(size_t)lzss->position & lzss->mask] = byte;
    lzss->position++;
}

/**
 * Emit a match (copy from history) into the window.
 * @param offset  Distance back from current position.
 * @param length  Number of bytes to copy.
 */
static inline void rar_lzss_emit_match(rar_lzss_t *lzss, int offset, int length)
{
    for(int i = 0; i < length; i++)
    {
        uint8_t byte = lzss->window[(size_t)(lzss->position - offset) & lzss->mask];
        lzss->window[(size_t)lzss->position & lzss->mask] = byte;
        lzss->position++;
    }
}

/**
 * Copy bytes from the LZSS window to an output buffer.
 * @param dest     Destination buffer.
 * @param start    Starting position in the LZSS stream.
 * @param length   Number of bytes to copy.
 */
void rar_lzss_copy_bytes(const rar_lzss_t *lzss, uint8_t *dest, int64_t start, size_t length);

/**
 * Get a pointer to the window data at a given position.
 * Only valid when the range doesn't wrap around the window boundary.
 */
static inline uint8_t *rar_lzss_window_ptr(const rar_lzss_t *lzss, int64_t pos)
{ return &lzss->window[(size_t)pos & lzss->mask]; }

/**
 * Return the next window edge (power-of-2 boundary) after a given position.
 */
static inline int64_t rar_lzss_next_window_edge(const rar_lzss_t *lzss, int64_t pos)
{ return (pos | (int64_t)lzss->mask) + 1; }

#endif /* AARU_COMPRESSION_NATIVE_RAR_LZSS_H */
