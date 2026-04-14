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

#ifndef AARU_COMPRESSION_NATIVE__LHA_HUFFMAN_H_
#define AARU_COMPRESSION_NATIVE__LHA_HUFFMAN_H_

#include <stdint.h>
#include <stdbool.h>
#include "bitio.h"

#define LHA_HUFFMAN_TABLE_SIZE 10

typedef struct
{
    int branches[2];
} lha_tree_node;

typedef struct
{
    uint32_t length;
    int32_t  value;
} lha_table_entry;

typedef struct lha_prefix_code
{
    lha_tree_node  *tree;
    int             numentries;
    int             minlength;
    int             maxlength;

    int             tablesize;
    lha_table_entry *table;
} lha_prefix_code;

lha_prefix_code *lha_prefix_code_new(void);
lha_prefix_code *lha_prefix_code_from_lengths(const int *lengths, int num_symbols, int max_length,
                                               bool shortest_is_zeros);
void             lha_prefix_code_free(lha_prefix_code *code);
void             lha_prefix_code_add(lha_prefix_code *code, int value, uint32_t codebits, int length);
int              lha_prefix_code_decode(lha_bitio *bitio, lha_prefix_code *code);

#endif /* AARU_COMPRESSION_NATIVE__LHA_HUFFMAN_H_ */
