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

/* StuffIt method 3: Static Huffman tree */

#include <stdlib.h>
#include "stuffit.h"
#include "stuffit_internal.h"

#define MAX_HUFFMAN_NODES 512

typedef struct HuffmanNode
{
    int left, right; /* -1 for leaf */
    int value;       /* leaf value, -1 for internal */
} HuffmanNode;

static int build_tree(StuffitBitReader *br, HuffmanNode *nodes, int *count)
{
    int idx = (*count)++;
    if(idx >= MAX_HUFFMAN_NODES) return -1;

    if(stuffit_bits_read_bit(br) == 1)
    {
        /* Leaf node */
        nodes[idx].left  = -1;
        nodes[idx].right = -1;
        nodes[idx].value = stuffit_bits_read(br, 8);
    }
    else
    {
        /* Internal node */
        nodes[idx].value = -1;
        nodes[idx].left  = build_tree(br, nodes, count);
        nodes[idx].right = build_tree(br, nodes, count);
        if(nodes[idx].left < 0 || nodes[idx].right < 0) return -1;
    }

    return idx;
}

int stuffit_huffman_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t           limit = *dst_size;
    size_t           di    = 0;
    StuffitBitReader br;
    stuffit_bits_init(&br, src, src_size);

    HuffmanNode nodes[MAX_HUFFMAN_NODES];
    int         node_count = 0;
    int         root       = build_tree(&br, nodes, &node_count);

    if(root < 0) return -1;

    while(di < limit)
    {
        int node = root;
        while(nodes[node].value < 0)
        {
            int bit = stuffit_bits_read_bit(&br);
            if(bit == 0)
                node = nodes[node].left;
            else
                node = nodes[node].right;

            if(node < 0 || node >= node_count) return -1;
        }
        dst[di++] = (uint8_t)nodes[node].value;
    }

    *dst_size = di;
    return 0;
}
