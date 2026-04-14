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

#include "lh1.h"
#include "bitio.h"
#include "huffman.h"
#include "lzss.h"

#include <string.h>

#define LH1_NUM_LEAVES 314
#define LH1_NUM_NODES  (LH1_NUM_LEAVES * 2 - 1)

typedef struct lh1_node
{
    struct lh1_node *parent;
    struct lh1_node *leftchild;
    struct lh1_node *rightchild;
    int              index; /* Position in sorted nodes[] array */
    int              freq;
    int              value; /* Symbol value for leaf nodes */
} lh1_node;

typedef struct
{
    lh1_node  storage[LH1_NUM_NODES];
    lh1_node *nodes[LH1_NUM_NODES]; /* Sorted by frequency order */
} lh1_tree;

static void lh1_init_tree(lh1_tree *tree)
{
    int i;

    memset(tree->storage, 0, sizeof(tree->storage));

    for(i = 0; i < LH1_NUM_NODES; i++) tree->nodes[i] = &tree->storage[i];

    /* Initialize leaf nodes (at the high end of the sorted array) */
    for(i = 0; i < LH1_NUM_LEAVES; i++)
    {
        int index              = LH1_NUM_NODES - 1 - i;
        tree->nodes[index]->index = index;
        tree->nodes[index]->freq  = 1;
        tree->nodes[index]->value = i;
        tree->nodes[index]->leftchild  = NULL;
        tree->nodes[index]->rightchild = NULL;
    }

    /* Build branch nodes bottom-up */
    for(i = LH1_NUM_LEAVES - 2; i >= 0; i--)
    {
        tree->nodes[i]->index      = i;
        tree->nodes[i]->leftchild  = tree->nodes[2 * i + 1];
        tree->nodes[i]->rightchild = tree->nodes[2 * i + 2];
        tree->nodes[i]->leftchild->parent  = tree->nodes[i];
        tree->nodes[i]->rightchild->parent = tree->nodes[i];
        tree->nodes[i]->freq = tree->nodes[i]->leftchild->freq + tree->nodes[i]->rightchild->freq;
    }

    tree->nodes[0]->parent = NULL;
}

static void lh1_rearrange(lh1_tree *tree, lh1_node *node)
{
    int       p_index = node->index;
    int       q_index = p_index;
    lh1_node *p = node;
    lh1_node *q;

    /* Find highest position where this node should be */
    while(q_index > 0 && tree->nodes[q_index - 1]->freq < p->freq) q_index--;

    if(q_index >= p_index) return;

    q = tree->nodes[q_index];

    /* Swap parent-child links using pointers */
    {
        lh1_node *new_q_parent = p->parent;
        lh1_node *new_p_parent = q->parent;
        int       p_is_right   = (p->parent && p->parent->rightchild == p);
        int       q_is_right   = (q->parent && q->parent->rightchild == q);

        if(p->parent)
        {
            if(p_is_right)
                p->parent->rightchild = q;
            else
                p->parent->leftchild = q;
        }

        if(q->parent)
        {
            if(q_is_right)
                q->parent->rightchild = p;
            else
                q->parent->leftchild = p;
        }

        p->parent = new_p_parent;
        q->parent = new_q_parent;
    }

    /* Swap positions in sorted array */
    tree->nodes[p_index]        = q;
    tree->nodes[p_index]->index = p_index;
    tree->nodes[q_index]        = p;
    tree->nodes[q_index]->index = q_index;
}

static void lh1_reconstruct(lh1_tree *tree)
{
    lh1_node *leaves[LH1_NUM_LEAVES];
    int       n = 0;
    int       i;
    int       leaf_index, branch_index, node_index, pair_index;

    /* Collect all leaf nodes and halve their frequencies */
    for(i = 0; i < LH1_NUM_NODES; i++)
    {
        if(!tree->nodes[i]->leftchild && !tree->nodes[i]->rightchild)
        {
            tree->nodes[i]->freq = (tree->nodes[i]->freq + 1) / 2;
            leaves[n++]          = tree->nodes[i];
        }
    }

    leaf_index   = LH1_NUM_LEAVES - 1;
    branch_index = LH1_NUM_LEAVES - 2;
    node_index   = LH1_NUM_NODES - 1;
    pair_index   = LH1_NUM_NODES - 2;

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
            lh1_node *branch    = &tree->storage[branch_index--];
            branch->leftchild   = tree->nodes[pair_index];
            branch->rightchild  = tree->nodes[pair_index + 1];
            branch->leftchild->parent  = branch;
            branch->rightchild->parent = branch;
            branch->freq = tree->nodes[pair_index]->freq + tree->nodes[pair_index + 1]->freq;

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

static void lh1_update(lh1_tree *tree, lh1_node *node)
{
    if(tree->nodes[0]->freq == 0x8000) lh1_reconstruct(tree);

    for(;;)
    {
        node->freq++;

        if(!node->parent) break;

        lh1_rearrange(tree, node);
        node = node->parent;
    }
}

AARU_EXPORT int AARU_CALL lha_decompress_lh1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    lha_bitio        bitio;
    lha_lzss         lzss;
    lh1_tree         tree;
    lha_prefix_code *distancecode;
    size_t           expected;
    int              i;

    static const int dist_lengths[64] = {3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6,
                                         6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
                                         7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                         8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    expected = *out_len;

    if(!lha_lzss_init(&lzss, 4096, out_buf, expected)) return -1;

    /* Pre-fill window with specific pattern */
    for(i = 0; i < 256; i++) memset(&lzss.window[i * 13 + 18], i, 13);

    for(i = 0; i < 256; i++) lzss.window[256 * 13 + 18 + i] = (uint8_t)i;

    for(i = 0; i < 256; i++) lzss.window[256 * 13 + 256 + 18 + i] = (uint8_t)(255 - i);

    memset(&lzss.window[256 * 13 + 512 + 18], 0, 128);
    memset(&lzss.window[256 * 13 + 512 + 128 + 18], ' ', 128 - 18);

    distancecode = lha_prefix_code_from_lengths(dist_lengths, 64, 8, true);

    if(!distancecode)
    {
        lha_lzss_cleanup(&lzss);
        return -1;
    }

    lha_bitio_init(&bitio, in_buf, in_len);
    lh1_init_tree(&tree);

    while(lzss.out_pos < expected && !lha_bitio_at_eof(&bitio))
    {
        /* Traverse adaptive Huffman tree */
        lh1_node *node = tree.nodes[0]; /* root */

        while(node->leftchild || node->rightchild)
        {
            if(lha_bitio_next_bit(&bitio))
                node = node->leftchild;
            else
                node = node->rightchild;

            if(!node) break;
        }

        if(!node) break;

        lh1_update(&tree, node);

        if(node->value < 0x100)
        {
            /* Literal */
            lha_lzss_emit_literal(&lzss, (uint8_t)node->value);
        }
        else
        {
            /* Match */
            int length   = node->value - 0x100 + 3;
            int highbits = lha_prefix_code_decode(&bitio, distancecode);
            int lowbits  = (int)lha_bitio_next_bits(&bitio, 6);
            int offset   = (highbits << 6) + lowbits + 1;

            lha_lzss_emit_match(&lzss, offset, length);
        }
    }

    *out_len = lzss.out_pos;
    lha_prefix_code_free(distancecode);
    lha_lzss_cleanup(&lzss);
    return 0;
}
