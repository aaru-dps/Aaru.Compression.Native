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

#include "huffman.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* --- Internal helpers --- */

static inline int left_branch(lha_prefix_code *c, int node) { return c->tree[node].branches[0]; }

static inline int right_branch(lha_prefix_code *c, int node) { return c->tree[node].branches[1]; }

static inline void set_branch(lha_prefix_code *c, int node, int bit, int next)
{
    c->tree[node].branches[bit] = next;
}

static inline bool is_leaf(lha_prefix_code *c, int node)
{
    return c->tree[node].branches[0] == c->tree[node].branches[1];
}

static inline int leaf_value(lha_prefix_code *c, int node) { return left_branch(c, node); }

static inline void set_leaf(lha_prefix_code *c, int node, int value)
{
    c->tree[node].branches[0] = value;
    c->tree[node].branches[1] = value;
}

static inline void set_empty(lha_prefix_code *c, int node)
{
    c->tree[node].branches[0] = -1;
    c->tree[node].branches[1] = -2;
}

static inline bool is_empty(lha_prefix_code *c, int node)
{
    return c->tree[node].branches[0] == -1 && c->tree[node].branches[1] == -2;
}

static inline bool is_open(lha_prefix_code *c, int node, int bit) { return c->tree[node].branches[bit] < 0; }

static int new_node(lha_prefix_code *c)
{
    lha_tree_node *tmp = (lha_tree_node *)realloc(c->tree, (size_t)(c->numentries + 1) * sizeof(lha_tree_node));

    if(!tmp) return -1;

    c->tree = tmp;
    set_empty(c, c->numentries);
    return c->numentries++;
}

/* --- Build lookup table --- */

static void make_table(lha_prefix_code *code, int node, lha_table_entry *table, int depth, int maxdepth)
{
    int currtablesize = 1 << (maxdepth - depth);

    if(node < 0)
    {
        int i;
        for(i = 0; i < currtablesize; i++) table[i].length = (uint32_t)-1;
    }
    else if(is_leaf(code, node))
    {
        int i;
        for(i = 0; i < currtablesize; i++)
        {
            table[i].length = (uint32_t)depth;
            table[i].value  = leaf_value(code, node);
        }
    }
    else
    {
        if(depth == maxdepth)
        {
            table[0].length = (uint32_t)(maxdepth + 1);
            table[0].value  = node;
        }
        else
        {
            make_table(code, left_branch(code, node), table, depth + 1, maxdepth);
            make_table(code, right_branch(code, node), table + currtablesize / 2, depth + 1, maxdepth);
        }
    }
}

static void build_table(lha_prefix_code *code)
{
    if(code->table) return;

    if(code->maxlength < code->minlength)
        code->tablesize = LHA_HUFFMAN_TABLE_SIZE;
    else if(code->maxlength >= LHA_HUFFMAN_TABLE_SIZE)
        code->tablesize = LHA_HUFFMAN_TABLE_SIZE;
    else
        code->tablesize = code->maxlength;

    code->table = (lha_table_entry *)malloc(sizeof(lha_table_entry) * (size_t)(1 << code->tablesize));

    if(code->table) make_table(code, 0, code->table, 0, code->tablesize);
}

/* --- Public API --- */

lha_prefix_code *lha_prefix_code_new(void)
{
    lha_prefix_code *code = (lha_prefix_code *)calloc(1, sizeof(lha_prefix_code));

    if(!code) return NULL;

    code->tree = (lha_tree_node *)malloc(sizeof(lha_tree_node));

    if(!code->tree)
    {
        free(code);
        return NULL;
    }

    set_empty(code, 0);
    code->numentries = 1;
    code->minlength  = INT_MAX;
    code->maxlength  = INT_MIN;
    code->table      = NULL;
    code->tablesize  = 0;

    return code;
}

lha_prefix_code *lha_prefix_code_from_lengths(const int *lengths, int num_symbols, int max_length,
                                               bool shortest_is_zeros)
{
    lha_prefix_code *code = lha_prefix_code_new();

    if(!code) return NULL;

    int      codeval     = 0;
    int      symbolsleft = num_symbols;
    int      length, i;

    for(length = 1; length <= max_length; length++)
    {
        for(i = 0; i < num_symbols; i++)
        {
            if(lengths[i] != length) continue;

            if(shortest_is_zeros)
                lha_prefix_code_add(code, i, (uint32_t)codeval, length);
            else
                lha_prefix_code_add(code, i, (uint32_t)~codeval, length);

            codeval++;

            if(--symbolsleft == 0) return code;
        }

        codeval <<= 1;
    }

    return code;
}

void lha_prefix_code_free(lha_prefix_code *code)
{
    if(!code) return;

    free(code->tree);
    free(code->table);
    free(code);
}

void lha_prefix_code_add(lha_prefix_code *code, int value, uint32_t codebits, int length)
{
    int bitpos, lastnode;

    /* Invalidate lookup table */
    free(code->table);
    code->table = NULL;

    if(length > code->maxlength) code->maxlength = length;
    if(length < code->minlength) code->minlength = length;

    lastnode = 0;

    for(bitpos = length - 1; bitpos >= 0; bitpos--)
    {
        int bit = (codebits >> bitpos) & 1;

        if(is_leaf(code, lastnode)) return; /* prefix collision */

        if(is_open(code, lastnode, bit)) set_branch(code, lastnode, bit, new_node(code));

        lastnode = code->tree[lastnode].branches[bit];

        if(lastnode < 0) return; /* allocation failure */
    }

    if(!is_empty(code, lastnode)) return; /* prefix collision */

    set_leaf(code, lastnode, value);
}

int lha_prefix_code_decode(lha_bitio *bitio, lha_prefix_code *code)
{
    int bits, length, value, node;

    if(!code) return -1;

    build_table(code);

    if(!code->table) return -1;

    bits   = (int)lha_bitio_peek_bits(bitio, code->tablesize);
    length = (int)code->table[bits].length;
    value  = code->table[bits].value;

    if(length == (int)(uint32_t)-1) return -1; /* invalid code */

    if(length <= code->tablesize)
    {
        lha_bitio_skip_bits(bitio, length);
        return value;
    }

    lha_bitio_skip_bits(bitio, code->tablesize);

    node = value;

    while(!is_leaf(code, node))
    {
        int bit = (int)lha_bitio_next_bit(bitio);

        if(is_open(code, node, bit)) return -1;

        node = code->tree[node].branches[bit];
    }

    return leaf_value(code, node);
}
