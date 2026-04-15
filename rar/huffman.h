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

#ifndef AARU_COMPRESSION_NATIVE_RAR_HUFFMAN_H
#define AARU_COMPRESSION_NATIVE_RAR_HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

#include "bitstream.h"

/**
 * Maximum code length supported by the Huffman decoder.
 */
#define RAR_HUFF_MAX_CODE_LEN 15

/**
 * Table-accelerated lookup bits. Codes <= this length are decoded with
 * a single table lookup. Longer codes fall back to a binary tree walk.
 */
#define RAR_HUFF_TABLE_BITS 10
#define RAR_HUFF_TABLE_SIZE (1 << RAR_HUFF_TABLE_BITS)

/**
 * Marker value for tree-based entries in the quick lookup table.
 * value field is -1 when the quick lookup is insufficient.
 */
#define RAR_HUFF_TREE_MARKER (-1)

/**
 * Quick lookup table entry. For codes up to RAR_HUFF_TABLE_BITS long,
 * length and value are stored directly. For longer codes, value is
 * RAR_HUFF_TREE_MARKER and tree walk is needed.
 */
typedef struct
{
    int16_t value;
    int16_t length;
} rar_huff_table_entry_t;

/**
 * Binary tree node for codes longer than the table lookup.
 * branch[0] = left (bit 0), branch[1] = right (bit 1).
 * Positive values are child node indices.
 * value >= 0 means this is a leaf node (decoded symbol).
 * value == -1 means this is an internal node.
 */
typedef struct
{
    int32_t branch[2];
    int32_t value; /* -1 = internal, >=0 = leaf symbol */
} rar_huff_tree_node_t;

/**
 * Huffman/prefix code decoder.
 *
 * Uses canonical Huffman code construction from code lengths.
 * MSB-first bit extraction (shortest code has lowest numeric value).
 */
typedef struct
{
    rar_huff_table_entry_t table[RAR_HUFF_TABLE_SIZE];
    rar_huff_tree_node_t  *tree;
    int                    tree_size;
    int                    tree_capacity;
    int                    num_symbols;
    int                    max_length;
} rar_huff_code_t;

/**
 * Create a Huffman code from an array of code lengths.
 *
 * @param code        Output structure (caller-allocated or heap-allocated).
 * @param lengths     Array of code lengths, one per symbol (0 = unused).
 * @param num_symbols Number of symbols (length of the lengths array).
 * @param max_length  Maximum code length (up to RAR_HUFF_MAX_CODE_LEN).
 * @return 0 on success, -1 on error.
 */
int rar_huff_create_from_lengths(rar_huff_code_t *code, const int *lengths, int num_symbols, int max_length);

/**
 * Create a Huffman code by manually adding individual codes.
 * Call rar_huff_init_empty() first, then rar_huff_add_code() for each symbol,
 * then rar_huff_build_table() to finalize.
 */
void rar_huff_init_empty(rar_huff_code_t *code, int max_length);

/**
 * Add a single code to the Huffman tree.
 * @param code_value  The code bits (MSB-first, right-aligned).
 * @param code_len    Length of the code in bits.
 * @param symbol      The symbol value this code represents.
 * @return 0 on success, -1 on error.
 */
int rar_huff_add_code(rar_huff_code_t *code, uint32_t code_value, int code_len, int symbol);

/**
 * Build the quick lookup table after manually adding codes.
 */
void rar_huff_build_table(rar_huff_code_t *code);

/**
 * Decode the next symbol from the bitstream.
 *
 * @param code  The Huffman code to use.
 * @param bs    The bitstream to read from.
 * @return The decoded symbol value, or -1 on error.
 */
int rar_huff_decode(const rar_huff_code_t *code, rar_bitstream_t *bs);

/**
 * Free resources allocated by rar_huff_create_from_lengths().
 */
void rar_huff_free(rar_huff_code_t *code);

#endif /* AARU_COMPRESSION_NATIVE_RAR_HUFFMAN_H */
