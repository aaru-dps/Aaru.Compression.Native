/*
 * cpt.h - Compact Pro archive decompression
 *
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

#ifndef AARU_CPT_H
#define AARU_CPT_H

#include <stddef.h>
#include <stdint.h>

/**
 * Decompress Compact Pro RLE-encoded data.
 *
 * All Compact Pro entries use RLE encoding with escape byte 0x81.
 *
 * @param dst_buffer Output buffer for decompressed data.
 * @param dst_size   On input, size of dst_buffer. On output, actual bytes written.
 * @param src_buffer Input RLE-encoded data.
 * @param src_size   Size of input data.
 * @return 0 on success, -1 on error.
 */
int cpt_rle_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size);

/**
 * Decompress Compact Pro LZH+RLE-encoded data.
 *
 * LZH-compressed entries use block-based canonical Huffman coding with an
 * 8192-byte LZSS sliding window, followed by RLE decoding.
 *
 * @param dst_buffer Output buffer for decompressed data.
 * @param dst_size   On input, size of dst_buffer. On output, actual bytes written.
 * @param src_buffer Input LZH+RLE compressed data.
 * @param src_size   Size of input data.
 * @return 0 on success, -1 on error.
 */
int cpt_lzh_rle_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size);

#endif /* AARU_CPT_H */
