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

/* StuffIt method 5: LZAH (LZH with adaptive Huffman tree, 4KB window) */

#include <stdlib.h>
#include <string.h>
#include "stuffit.h"
#include "stuffit_internal.h"

#define LZAH_NUM_LEAVES  314
#define LZAH_NUM_NODES   (LZAH_NUM_LEAVES * 2 - 1)
#define LZAH_WINDOW_SIZE 4096

typedef struct LzahNode
{
    int      parent;
    int      left, right;
    uint32_t freq;
    int      value; /* -1 for internal, 0..313 for leaf */
    int      index; /* position in sorted nodes array */
} LzahNode;

typedef struct LzahState
{
    LzahNode  storage[LZAH_NUM_NODES];
    LzahNode *nodes[LZAH_NUM_NODES];
    uint8_t   window[LZAH_WINDOW_SIZE];
} LzahState;

static void lzah_rearrange(LzahState *st, LzahNode *node);
static void lzah_reconstruct(LzahState *st);

/* Distance code table: 64 entries, code lengths 3-8 */
static const int dist_code_lengths[64] = {
    3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

static int decode_distance_code(StuffitBitReader *br)
{
    /* Build prefix lookup: traverse one bit at a time MSB-first */
    int val      = 0;
    int len      = 0;
    int code_idx = -1;

    /* Use a compact lookup: decode from MSB-first canonical codes */
    /* The distance code table is equivalent to a canonical Huffman with
       code lengths as given above. Build a simple tree-based decoder. */
    /* Simpler approach: accumulate bits and compare. The table is ordered
       by code length, and within each length by symbol order. */

    /* Pre-built lookup: symbol from bit accumulation */
    int sym        = 0;
    int code       = 0;
    int first_code = 0; /* first code of current length */
    int first_sym  = 0; /* first symbol of current length */

    for(int cl = 3; cl <= 8; cl++)
    {
        code <<= 1;
        code |= stuffit_bits_read_bit(br);

        /* Count symbols with this code length */
        int count = 0;
        for(int i = 0; i < 64; i++)
            if(dist_code_lengths[i] == cl) count++;

        if(code - first_code < count) return first_sym + (code - first_code);

        first_code = (first_code + count) << 1;
        first_sym += count;
    }

    return 0;
}

/* Alternative: Rebuild a proper prefix code decoder for the 64-symbol distance code */
static int decode_dist_from_table(StuffitBitReader *br)
{
    /* Build sorted order of symbols by code length, then by index */
    /* Since this is fixed, precompute the canonical codes */
    static int initialized  = 0;
    static int max_code_len = 8;
    /* For canonical Huffman: sort by (length, symbol), assign codes */
    /* Lengths: 3-bit: 0..0 (1 sym), wait it's 32 symbols of len 3 */
    /* Actually look at the data: 32 entries of 3, 32 of 4, etc. No. */
    /* 3: indices 0-0 = 1 symbol? No, look again: */
    /* 3,4,4,4,5,5,5,5,5,5,5,5,6...  so len 3 has 1 symbol (idx 0) */
    /* len 4 has 3 symbols (idx 1-3) */
    /* len 5 has 8 symbols (idx 4-11) */
    /* len 6 has 12 symbols (idx 12-23) */
    /* len 7 has 24 symbols (idx 24-47) */
    /* len 8 has 16 symbols (idx 48-63) */

    int val = 0;
    /* Read 3 bits */
    val     = stuffit_bits_read(br, 3);
    /* First code of len 3 is 0, so code 0 = symbol 0 */
    if(val == 0) return 0;
    /* Next: 3 symbols of len 4. First code of len 4 = (0+1)<<1 = 2 */
    val = (val << 1) | stuffit_bits_read_bit(br);
    if(val >= 2 && val < 5) return 1 + (val - 2);
    /* 8 symbols of len 5. First code of len 5 = (2+3)<<1 = 10 */
    val = (val << 1) | stuffit_bits_read_bit(br);
    if(val >= 10 && val < 18) return 4 + (val - 10);
    /* 12 symbols of len 6. First code of len 6 = (10+8)<<1 = 36 */
    val = (val << 1) | stuffit_bits_read_bit(br);
    if(val >= 36 && val < 48) return 12 + (val - 36);
    /* 24 symbols of len 7. First code of len 7 = (36+12)<<1 = 96 */
    val = (val << 1) | stuffit_bits_read_bit(br);
    if(val >= 96 && val < 120) return 24 + (val - 96);
    /* 16 symbols of len 8. First code of len 8 = (96+24)<<1 = 240 */
    val = (val << 1) | stuffit_bits_read_bit(br);
    if(val >= 240 && val < 256) return 48 + (val - 240);

    return 0;
}

int stuffit_lzah_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    LzahState *st = calloc(1, sizeof(LzahState));
    if(!st) return -1;

    StuffitBitReader br;
    stuffit_bits_init(&br, src, src_size);

    /* Initialize Huffman tree */
    for(int i = 0; i < LZAH_NUM_NODES; i++) st->nodes[i] = &st->storage[i];

    for(int i = 0; i < LZAH_NUM_LEAVES; i++)
    {
        int idx               = LZAH_NUM_NODES - 1 - i;
        st->nodes[idx]->index = idx;
        st->nodes[idx]->freq  = 1;
        st->nodes[idx]->value = i;
        st->nodes[idx]->left  = -1;
        st->nodes[idx]->right = -1;
    }

    for(int i = LZAH_NUM_LEAVES - 2; i >= 0; i--)
    {
        int li = 2 * i + 1, ri = 2 * i + 2;
        st->nodes[i]->index    = i;
        st->nodes[i]->left     = li;
        st->nodes[i]->right    = ri;
        st->nodes[i]->value    = -1;
        st->storage[li].parent = i;
        st->storage[ri].parent = i;
        st->nodes[i]->freq     = st->nodes[li]->freq + st->nodes[ri]->freq;
    }
    st->storage[0].parent = -1;

    /* Initialize window with known pattern */
    int wp = 0;
    for(int i = 0; i < 256; i++)
        for(int j = 0; j < 13; j++) st->window[(wp++ + 18) & (LZAH_WINDOW_SIZE - 1)] = (uint8_t)i;
    for(int i = 0; i < 256; i++) st->window[(wp++ + 18) & (LZAH_WINDOW_SIZE - 1)] = (uint8_t)i;
    for(int i = 0; i < 256; i++) st->window[(wp++ + 18) & (LZAH_WINDOW_SIZE - 1)] = (uint8_t)(255 - i);
    for(int i = 0; i < 128; i++) st->window[(wp++ + 18) & (LZAH_WINDOW_SIZE - 1)] = 0;
    for(int i = 0; i < 128 - 18; i++) st->window[(wp++ + 18) & (LZAH_WINDOW_SIZE - 1)] = ' ';

    int win_pos = 0;

    while(di < limit)
    {
        /* Decode a symbol from the adaptive Huffman tree */
        LzahNode *node = &st->storage[0];
        while(node->left >= 0 || node->right >= 0)
        {
            if(stuffit_bits_read_bit(&br))
                node = st->nodes[node->left];
            else
                node = st->nodes[node->right];

            if(!node)
            {
                free(st);
                return -1;
            }
        }

        int symbol = node->value;

        /* Update tree frequencies */
        if(st->storage[0].freq >= 0x8000) lzah_reconstruct(st);

        /* Walk up the tree, incrementing frequencies and rearranging */
        LzahNode *n = node;
        for(;;)
        {
            n->freq++;
            if(n->parent < 0) break;

            /* Rearrange: swap with higher-indexed node if needed */
            int p_idx = n->index;
            int q_idx = p_idx;
            while(q_idx > 0 && st->nodes[q_idx - 1]->freq < n->freq) q_idx--;

            if(q_idx < p_idx)
            {
                LzahNode *q = st->nodes[q_idx];

                /* Swap parent linkage */
                int n_parent   = n->parent;
                int q_parent   = q->parent;
                int n_is_right = (st->storage[n_parent].right == p_idx) ? 1 : 0;
                int q_is_right = (st->storage[q_parent].right == q_idx) ? 1 : 0;

                if(n_is_right)
                    st->storage[n_parent].right = q_idx;
                else
                    st->storage[n_parent].left = q_idx;

                if(q_is_right)
                    st->storage[q_parent].right = p_idx;
                else
                    st->storage[q_parent].left = p_idx;

                n->parent = q_parent;
                q->parent = n_parent;

                st->nodes[p_idx] = q;
                q->index         = p_idx;
                st->nodes[q_idx] = n;
                n->index         = q_idx;
            }

            n = &st->storage[n->parent];
        }

        if(symbol < 0x100)
        {
            /* Literal byte */
            dst[di++]           = (uint8_t)symbol;
            st->window[win_pos] = (uint8_t)symbol;
            win_pos             = (win_pos + 1) & (LZAH_WINDOW_SIZE - 1);
        }
        else
        {
            /* Match: length + distance */
            int length = symbol - 0x100 + 3;

            int highbits = decode_dist_from_table(&br);
            int lowbits  = stuffit_bits_read(&br, 6);
            int offset   = (highbits << 6) + lowbits + 1;

            int src_pos = (win_pos - offset + LZAH_WINDOW_SIZE) & (LZAH_WINDOW_SIZE - 1);
            for(int j = 0; j < length && di < limit; j++)
            {
                uint8_t b           = st->window[src_pos];
                dst[di++]           = b;
                st->window[win_pos] = b;
                win_pos             = (win_pos + 1) & (LZAH_WINDOW_SIZE - 1);
                src_pos             = (src_pos + 1) & (LZAH_WINDOW_SIZE - 1);
            }
        }
    }

    free(st);
    *dst_size = di;
    return 0;
}

static void lzah_reconstruct(LzahState *st)
{
    /* Collect leaves, halve their frequencies */
    LzahNode *leaves[LZAH_NUM_LEAVES];
    int       nleaves = 0;

    for(int i = 0; i < LZAH_NUM_NODES; i++)
    {
        if(st->nodes[i]->left < 0 && st->nodes[i]->right < 0)
        {
            st->nodes[i]->freq = (st->nodes[i]->freq + 1) / 2;
            leaves[nleaves++]  = st->nodes[i];
        }
    }

    /* Rebuild tree bottom-up */
    int leaf_idx   = LZAH_NUM_LEAVES - 1;
    int branch_idx = LZAH_NUM_LEAVES - 2;
    int node_idx   = LZAH_NUM_NODES - 1;
    int pair_idx   = LZAH_NUM_NODES - 2;

    while(node_idx >= 0)
    {
        while(node_idx >= pair_idx)
        {
            st->nodes[node_idx]        = leaves[leaf_idx];
            st->nodes[node_idx]->index = node_idx;
            node_idx--;
            leaf_idx--;
        }

        LzahNode *branch                 = &st->storage[branch_idx--];
        branch->left                     = pair_idx;
        branch->right                    = pair_idx + 1;
        branch->value                    = -1;
        st->storage[pair_idx].parent     = branch_idx + 1;
        st->storage[pair_idx + 1].parent = branch_idx + 1;
        branch->freq                     = st->nodes[pair_idx]->freq + st->nodes[pair_idx + 1]->freq;

        while(leaf_idx >= 0 && leaves[leaf_idx]->freq <= branch->freq)
        {
            st->nodes[node_idx]        = leaves[leaf_idx];
            st->nodes[node_idx]->index = node_idx;
            node_idx--;
            leaf_idx--;
        }

        st->nodes[node_idx]        = branch;
        st->nodes[node_idx]->index = node_idx;
        node_idx--;
        pair_idx -= 2;
    }
    st->nodes[0]->parent = -1;
}
