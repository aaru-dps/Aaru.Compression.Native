/*
 * dd.h - DiskDoubler archive decompression
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

#ifndef AARU_DD_H
#define AARU_DD_H

#include <stddef.h>
#include <stdint.h>

/**
 * Decompress DiskDoubler ADn block-based LZSS data (methods 6 and 9).
 *
 * The compressed stream consists of sequential blocks, each with a 12-byte
 * header followed by compressed or uncompressed block data. Each block
 * decompresses to at most 8192 bytes.
 *
 * The LZSS encoding uses MSB-first bits: bit=0 means literal (8 bits),
 * bit=1 means match. Matches encode a near/far flag, offset (8 or 12 bits),
 * and variable-length match length (2-20 bytes).
 *
 * @param dst_buffer  Output buffer for decompressed data.
 * @param dst_size    On entry, capacity of output buffer.
 *                    On return, number of bytes actually written.
 * @param src_buffer  Input compressed data.
 * @param src_size    Size of input data in bytes.
 * @return 0 on success, -1 on error.
 */
int dd_adn_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size);

/**
 * Decompress DiskDoubler DDn block-based Huffman LZ77 data (method 10).
 *
 * The compressed stream consists of sequential blocks, each with a 22-byte
 * header followed by Huffman code tables and compressed data. Uses a 64 KB
 * sliding window. Each block contains separately coded offset tables,
 * literal data, and length codes.
 *
 * @param dst_buffer  Output buffer for decompressed data.
 * @param dst_size    On entry, capacity of output buffer.
 *                    On return, number of bytes actually written.
 * @param src_buffer  Input compressed data.
 * @param src_size    Size of input data in bytes.
 * @return 0 on success, -1 on error.
 */
int dd_ddn_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size);

/**
 * Decompress DiskDoubler Method 2 adaptive Huffman data.
 *
 * Uses a set of splay trees (one per byte value modulo numtrees) that
 * adapt during decompression. Each output byte is decoded by walking a
 * binary tree using input bits, then the tree is restructured to move
 * frequently-used symbols closer to the root.
 *
 * Method 5 is a variant that reads the number of trees from the first
 * byte of the stream (0 means 256). Method 2 always uses 256 trees.
 *
 * @param dst_buffer  Output buffer for decompressed data.
 * @param dst_size    On entry, capacity of output buffer.
 *                    On return, number of bytes actually written.
 * @param src_buffer  Input compressed data.
 * @param src_size    Size of input data in bytes.
 * @param num_trees   Number of splay trees (1-256, typically 256).
 * @return 0 on success, -1 on error.
 */
int dd_method2_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size,
                             int num_trees);

/**
 * Decompress Stac LZS data (DiskDoubler method 7).
 *
 * LZSS with a 2 KB sliding window. Match lengths are unbounded and may
 * exceed the window size. Uses MSB-first bits. Offsets are either 7-bit
 * (short) or 11-bit (long). Lengths use a fixed Huffman table for values
 * 2-8, with extension nibbles for longer matches. A zero high-offset byte
 * signals end of stream.
 *
 * @param dst_buffer  Output buffer for decompressed data.
 * @param dst_size    On entry, capacity of output buffer.
 *                    On return, number of bytes actually written.
 * @param src_buffer  Input compressed data.
 * @param src_size    Size of input data in bytes.
 * @return 0 on success, -1 on error.
 */
int dd_stac_lzs_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size);

/**
 * Decompress DiskDoubler Compact Pro data (method 8).
 *
 * Reads a 16-byte header; if the byte sum is zero, applies LZH + RLE
 * decompression, otherwise applies RLE only. Delegates to the existing
 * Compact Pro decompressor functions.
 *
 * @param dst_buffer  Output buffer for decompressed data.
 * @param dst_size    On entry, capacity of output buffer.
 *                    On return, number of bytes actually written.
 * @param src_buffer  Input compressed data (including 16-byte header).
 * @param src_size    Size of input data in bytes.
 * @return 0 on success, -1 on error.
 */
int dd_cpt_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size);

/**
 * Decompress DiskDoubler LZW data (method 1, Unix compress variant).
 *
 * Variable-width LZW with LSB-first bit reading, 9 to maxbits code width.
 * The flags byte encodes: bits 0-4 = max code bits (9-16),
 * bit 7 = block mode (code 256 triggers table clear).
 *
 * The caller is responsible for stripping the 3-byte header (m1, m2, flags)
 * and any XOR decryption before calling this function.
 *
 * @param dst_buffer  Output buffer for decompressed data.
 * @param dst_size    On entry, capacity of output buffer.
 *                    On return, number of bytes actually written.
 * @param src_buffer  Input LZW-compressed data (after the 3-byte header).
 * @param src_size    Size of input data in bytes.
 * @param flags       The flags byte from the 3-byte header.
 * @return 0 on success, -1 on error.
 */
int dd_lzw_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size, int flags);

#endif /* AARU_DD_H */
