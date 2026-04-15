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

#include <stdlib.h>
#include <string.h>

#include "huffman.h"

/*
 * Allocate a new tree node, return its index.
 */
static int alloc_tree_node(rar_huff_code_t *code)
{
    if(code->tree_size >= code->tree_capacity)
    {
        int                   new_cap  = code->tree_capacity ? code->tree_capacity * 2 : 64;
        rar_huff_tree_node_t *new_tree = realloc(code->tree, (size_t)new_cap * sizeof(rar_huff_tree_node_t));
        if(!new_tree) return -1;
        code->tree          = new_tree;
        code->tree_capacity = new_cap;
    }
    int idx                   = code->tree_size++;
    code->tree[idx].branch[0] = 0;
    code->tree[idx].branch[1] = 0;
    code->tree[idx].value     = -1; /* internal node */
    return idx;
}

void rar_huff_init_empty(rar_huff_code_t *code, int max_length)
{
    memset(code, 0, sizeof(*code));
    code->max_length = max_length;
    /* Initialize table entries to empty */
    for(int i = 0; i < RAR_HUFF_TABLE_SIZE; i++)
    {
        code->table[i].value  = RAR_HUFF_TREE_MARKER;
        code->table[i].length = 0;
    }
    /* Allocate root node */
    alloc_tree_node(code);
}

int rar_huff_add_code(rar_huff_code_t *code, uint32_t code_value, int code_len, int symbol)
{
    if(code_len <= 0 || code_len > RAR_HUFF_MAX_CODE_LEN) return -1;

    /* Walk / create tree nodes */
    int node = 0;
    for(int i = code_len - 1; i >= 0; i--)
    {
        int bit = (code_value >> i) & 1;
        if(code->tree[node].branch[bit] == 0)
        {
            int child = alloc_tree_node(code);
            if(child < 0) return -1;
            code->tree[node].branch[bit] = child;
        }
        node = code->tree[node].branch[bit];
    }
    /* Mark leaf */
    code->tree[node].value = symbol;
    return 0;
}

void rar_huff_build_table(rar_huff_code_t *code)
{
    /* Build quick lookup table by walking all 2^TABLE_BITS patterns */
    for(int i = 0; i < RAR_HUFF_TABLE_SIZE; i++)
    {
        int node = 0;
        int len  = 0;
        for(int bit = RAR_HUFF_TABLE_BITS - 1; bit >= 0; bit--)
        {
            int b    = (i >> bit) & 1;
            int next = code->tree[node].branch[b];
            len++;

            if(next == 0)
            {
                /* Dead end — no valid code */
                code->table[i].value  = RAR_HUFF_TREE_MARKER;
                code->table[i].length = (int16_t)RAR_HUFF_TABLE_BITS;
                goto next_entry;
            }

            node = next;

            if(code->tree[node].value >= 0)
            {
                /* Reached a leaf */
                code->table[i].value  = (int16_t)code->tree[node].value;
                code->table[i].length = (int16_t)len;
                goto next_entry;
            }
        }
        /* code is longer than TABLE_BITS — mark for tree walk */
        code->table[i].value  = RAR_HUFF_TREE_MARKER;
        code->table[i].length = (int16_t)RAR_HUFF_TABLE_BITS;
    next_entry:;
    }
}

int rar_huff_create_from_lengths(rar_huff_code_t *code, const int *lengths, int num_symbols, int max_length)
{
    uint32_t code_counts[RAR_HUFF_MAX_CODE_LEN + 1];
    uint32_t next_code[RAR_HUFF_MAX_CODE_LEN + 1];

    memset(code, 0, sizeof(*code));
    code->num_symbols = num_symbols;
    code->max_length  = max_length > RAR_HUFF_MAX_CODE_LEN ? RAR_HUFF_MAX_CODE_LEN : max_length;

    /* Count symbols per code length */
    memset(code_counts, 0, sizeof(code_counts));
    for(int i = 0; i < num_symbols; i++)
    {
        if(lengths[i] > 0 && lengths[i] <= code->max_length) code_counts[lengths[i]]++;
    }

    /* Compute first code for each length (canonical Huffman) */
    memset(next_code, 0, sizeof(next_code));
    uint32_t c = 0;
    for(int bits = 1; bits <= code->max_length; bits++)
    {
        c               = (c + code_counts[bits - 1]) << 1;
        next_code[bits] = c;
    }

    /* Initialize table to empty */
    for(int i = 0; i < RAR_HUFF_TABLE_SIZE; i++)
    {
        code->table[i].value  = RAR_HUFF_TREE_MARKER;
        code->table[i].length = 0;
    }

    /* Allocate root tree node */
    if(alloc_tree_node(code) < 0) return -1;

    /* Assign canonical codes and insert into tree */
    for(int sym = 0; sym < num_symbols; sym++)
    {
        int len = lengths[sym];
        if(len <= 0 || len > code->max_length) continue;

        uint32_t cv = next_code[len]++;
        if(rar_huff_add_code(code, cv, len, sym) < 0)
        {
            rar_huff_free(code);
            return -1;
        }
    }

    /* Build the quick lookup table */
    rar_huff_build_table(code);

    return 0;
}

int rar_huff_decode(const rar_huff_code_t *code, rar_bitstream_t *bs)
{
    rar_huff_table_entry_t entry;

    /* Peek TABLE_BITS bits without consuming */
    rar_bs_refill_inline(bs);
    uint32_t peek = bs->bit_buf >> (32 - RAR_HUFF_TABLE_BITS);
    entry         = code->table[peek];

    if(entry.value != RAR_HUFF_TREE_MARKER)
    {
        /* Quick lookup hit: consume entry.length bits */
        bs->bit_buf <<= entry.length;
        bs->bits_left -= entry.length;
        return entry.value;
    }

    /* Slow path: walk the tree for codes longer than TABLE_BITS */
    /* Consume the TABLE_BITS bits we already peeked */
    bs->bit_buf <<= RAR_HUFF_TABLE_BITS;
    bs->bits_left -= RAR_HUFF_TABLE_BITS;

    /* Find the tree node we're already at after table_bits of peeking */
    int      node = 0;
    uint32_t p    = peek;
    for(int i = RAR_HUFF_TABLE_BITS - 1; i >= 0; i--)
    {
        int bit = (p >> i) & 1;
        node    = code->tree[node].branch[bit];
        if(node == 0 || code->tree[node].value >= 0) return code->tree[node].value;
    }

    /* Continue walking */
    for(int i = RAR_HUFF_TABLE_BITS; i < code->max_length; i++)
    {
        int bit = rar_bs_read_bit(bs);
        node    = code->tree[node].branch[bit];
        if(node == 0 || code->tree[node].value >= 0) return code->tree[node].value;
    }

    return -1; /* error: code too long */
}

void rar_huff_free(rar_huff_code_t *code)
{
    if(code->tree)
    {
        free(code->tree);
        code->tree          = NULL;
        code->tree_size     = 0;
        code->tree_capacity = 0;
    }
}
