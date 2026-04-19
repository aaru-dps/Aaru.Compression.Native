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

/*
 * Apple LZH decompression — used by DART disk image format "best" compression.
 *
 * This is identical to LHA -lh1- (adaptive Huffman + LZSS, 4KB window) except
 * the LZSS window is pre-filled with zeros in the last 110 bytes rather than
 * spaces (0x20). The standard LH1 format uses spaces as specified by Okumura's
 * original LZHUF.C; Apple's DART tool uses zeros instead.
 *
 * Reference: XADLZHDynamicHandle from The Unarchiver with prefill:NO
 */

#include "apple_lzh.h"
#include "lha/bitio.h"
#include "lha/huffman.h"
#include "lha/lzss.h"

#include <string.h>

#define APPLE_LZH_NUM_LEAVES 314
#define APPLE_LZH_NUM_NODES  (APPLE_LZH_NUM_LEAVES * 2 - 1)

typedef struct apple_lzh_node
{
    struct apple_lzh_node *parent;
    struct apple_lzh_node *leftchild;
    struct apple_lzh_node *rightchild;
    int                    index;
    int                    freq;
    int                    value;
} apple_lzh_node;

typedef struct
{
    apple_lzh_node  storage[APPLE_LZH_NUM_NODES];
    apple_lzh_node *nodes[APPLE_LZH_NUM_NODES];
} apple_lzh_tree;

static void apple_lzh_init_tree(apple_lzh_tree *tree)
{
    int i;

    memset(tree->storage, 0, sizeof(tree->storage));

    for(i = 0; i < APPLE_LZH_NUM_NODES; i++) tree->nodes[i] = &tree->storage[i];

    for(i = 0; i < APPLE_LZH_NUM_LEAVES; i++)
    {
        int index                      = APPLE_LZH_NUM_NODES - 1 - i;
        tree->nodes[index]->index      = index;
        tree->nodes[index]->freq       = 1;
        tree->nodes[index]->value      = i;
        tree->nodes[index]->leftchild  = NULL;
        tree->nodes[index]->rightchild = NULL;
    }

    for(i = APPLE_LZH_NUM_LEAVES - 2; i >= 0; i--)
    {
        tree->nodes[i]->index              = i;
        tree->nodes[i]->leftchild          = tree->nodes[2 * i + 1];
        tree->nodes[i]->rightchild         = tree->nodes[2 * i + 2];
        tree->nodes[i]->leftchild->parent  = tree->nodes[i];
        tree->nodes[i]->rightchild->parent = tree->nodes[i];
        tree->nodes[i]->freq               = tree->nodes[i]->leftchild->freq + tree->nodes[i]->rightchild->freq;
    }

    tree->nodes[0]->parent = NULL;
}

static void apple_lzh_rearrange(apple_lzh_tree *tree, apple_lzh_node *node)
{
    int             p_index = node->index;
    int             q_index = p_index;
    apple_lzh_node *q;

    while(q_index > 0 && tree->nodes[q_index - 1]->freq < node->freq) q_index--;

    if(q_index >= p_index) return;

    q = tree->nodes[q_index];

    {
        apple_lzh_node *new_q_parent = node->parent;
        apple_lzh_node *new_p_parent = q->parent;
        int             p_is_right   = (node->parent && node->parent->rightchild == node);
        int             q_is_right   = (q->parent && q->parent->rightchild == q);

        if(node->parent)
        {
            if(p_is_right)
                node->parent->rightchild = q;
            else
                node->parent->leftchild = q;
        }

        if(q->parent)
        {
            if(q_is_right)
                q->parent->rightchild = node;
            else
                q->parent->leftchild = node;
        }

        node->parent = new_p_parent;
        q->parent    = new_q_parent;
    }

    tree->nodes[p_index]        = q;
    tree->nodes[p_index]->index = p_index;
    tree->nodes[q_index]        = node;
    tree->nodes[q_index]->index = q_index;
}

static void apple_lzh_reconstruct(apple_lzh_tree *tree)
{
    apple_lzh_node *leaves[APPLE_LZH_NUM_LEAVES];
    int             n = 0;
    int             i;
    int             leaf_index, branch_index, node_index, pair_index;

    for(i = 0; i < APPLE_LZH_NUM_NODES; i++)
    {
        if(!tree->nodes[i]->leftchild && !tree->nodes[i]->rightchild)
        {
            tree->nodes[i]->freq = (tree->nodes[i]->freq + 1) / 2;
            leaves[n++]          = tree->nodes[i];
        }
    }

    leaf_index   = APPLE_LZH_NUM_LEAVES - 1;
    branch_index = APPLE_LZH_NUM_LEAVES - 2;
    node_index   = APPLE_LZH_NUM_NODES - 1;
    pair_index   = APPLE_LZH_NUM_NODES - 2;

    while(node_index >= 0)
    {
        while(node_index >= pair_index)
        {
            tree->nodes[node_index]        = leaves[leaf_index];
            tree->nodes[node_index]->index = node_index;
            node_index--;
            leaf_index--;
        }

        {
            apple_lzh_node *branch     = &tree->storage[branch_index--];
            branch->leftchild          = tree->nodes[pair_index];
            branch->rightchild         = tree->nodes[pair_index + 1];
            branch->leftchild->parent  = branch;
            branch->rightchild->parent = branch;
            branch->freq               = tree->nodes[pair_index]->freq + tree->nodes[pair_index + 1]->freq;

            while(leaf_index >= 0 && leaves[leaf_index]->freq <= branch->freq)
            {
                tree->nodes[node_index]        = leaves[leaf_index];
                tree->nodes[node_index]->index = node_index;
                node_index--;
                leaf_index--;
            }

            tree->nodes[node_index]        = branch;
            tree->nodes[node_index]->index = node_index;
            node_index--;
            pair_index -= 2;
        }
    }

    tree->nodes[0]->parent = NULL;
}

static void apple_lzh_update(apple_lzh_tree *tree, apple_lzh_node *node)
{
    if(tree->nodes[0]->freq == 0x8000) apple_lzh_reconstruct(tree);

    for(;;)
    {
        node->freq++;

        if(!node->parent) break;

        apple_lzh_rearrange(tree, node);
        node = node->parent;
    }
}

AARU_EXPORT int AARU_CALL AARU_apple_lzh_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                       size_t src_size)
{
    lha_bitio        bitio;
    lha_lzss         lzss;
    apple_lzh_tree   tree;
    lha_prefix_code *distancecode;
    size_t           expected;
    int              i;

    static const int dist_lengths[64] = {3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                         6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                         7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

    if(!dst_buffer || !dst_size || !src_buffer || src_size == 0) return -1;

    expected = *dst_size;

    if(!lha_lzss_init(&lzss, 4096, dst_buffer, expected)) return -1;

    /* Pre-fill window with pattern identical to LH1... */
    for(i = 0; i < 256; i++) memset(&lzss.window[i * 13 + 18], i, 13);

    for(i = 0; i < 256; i++) lzss.window[256 * 13 + 18 + i] = (uint8_t)i;

    for(i = 0; i < 256; i++) lzss.window[256 * 13 + 256 + 18 + i] = (uint8_t)(255 - i);

    memset(&lzss.window[256 * 13 + 512 + 18], 0, 128);

    /* Apple variant: zeros instead of spaces for last 110 bytes */
    memset(&lzss.window[256 * 13 + 512 + 128 + 18], 0, 128 - 18);

    distancecode = lha_prefix_code_from_lengths(dist_lengths, 64, 8, true);

    if(!distancecode)
    {
        lha_lzss_cleanup(&lzss);
        return -1;
    }

    lha_bitio_init(&bitio, src_buffer, src_size);
    apple_lzh_init_tree(&tree);

    while(lzss.out_pos < expected && !lha_bitio_at_eof(&bitio))
    {
        apple_lzh_node *node = tree.nodes[0];

        while(node->leftchild || node->rightchild)
        {
            if(lha_bitio_next_bit(&bitio))
                node = node->leftchild;
            else
                node = node->rightchild;

            if(!node) break;
        }

        if(!node) break;

        apple_lzh_update(&tree, node);

        if(node->value < 0x100) { lha_lzss_emit_literal(&lzss, (uint8_t)node->value); }
        else
        {
            int length   = node->value - 0x100 + 3;
            int highbits = lha_prefix_code_decode(&bitio, distancecode);
            int lowbits  = (int)lha_bitio_next_bits(&bitio, 6);
            int offset   = (highbits << 6) + lowbits + 1;

            lha_lzss_emit_match(&lzss, offset, length);
        }
    }

    *dst_size = lzss.out_pos;
    lha_prefix_code_free(distancecode);
    lha_lzss_cleanup(&lzss);
    return 0;
}
