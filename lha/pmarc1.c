/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2026 Natalia Portillo.
 *
 * Based on code by Simon Howard, ISC license:
 * Copyright (c) 2011, 2012, Simon Howard
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
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

#include "pmarc1.h"
#include "bitio.h"
#include "lzss.h"

#include <stdbool.h>
#include <string.h>

#define MAX_BYTE_BLOCK_LEN 216

typedef struct
{
    uint8_t prev;
    uint8_t next;
} pma1_history_node;

typedef struct
{
    pma1_history_node history[256];
    uint8_t           history_head;
} pma1_history_list;

typedef struct
{
    unsigned int offset;
    unsigned int bits;
} variable_length_table;

/* --- History list --- */

static void init_history_list(pma1_history_list *list)
{
    int i;

    for(i = 0; i < 256; i++)
    {
        list->history[i].prev = (uint8_t)(i + 1);
        list->history[i].next = (uint8_t)(i - 1);
    }

    list->history_head = 0x20;

    list->history[0x7f].prev = 0x00;
    list->history[0x00].next = 0x7f;
    list->history[0x1f].prev = 0xa0;
    list->history[0xa0].next = 0x1f;
    list->history[0xdf].prev = 0x80;
    list->history[0x80].next = 0xdf;
    list->history[0x9f].prev = 0xe0;
    list->history[0xe0].next = 0x9f;
    list->history[0xff].prev = 0x20;
    list->history[0x20].next = 0xff;
}

static uint8_t find_in_history_list(pma1_history_list *list, uint8_t count)
{
    uint8_t code = list->history_head;
    int     i;

    if(count < 128)
    {
        for(i = 0; i < count; i++) code = list->history[code].prev;
    }
    else
    {
        for(i = 0; i < 256 - count; i++) code = list->history[code].next;
    }

    return code;
}

static void update_history_list(pma1_history_list *list, uint8_t b)
{
    pma1_history_node *node, *old_head;

    if(list->history_head == b) return;

    node     = &list->history[b];
    old_head = &list->history[list->history_head];

    /* Unhook from current position */
    list->history[node->next].prev = node->prev;
    list->history[node->prev].next = node->next;

    /* Hook between old head and its next */
    node->prev                       = list->history_head;
    node->next                       = old_head->next;
    list->history[old_head->next].prev = b;
    old_head->next                    = b;

    list->history_head = b;
}

/* --- Variable length tables --- */

static const variable_length_table copy_ranges[] = {
    {0, 6},     /* 0: 0 + (1<<6) = 64 */
    {64, 8},    /* 1: 64 + (1<<8) = 320 */
    {0, 6},     /* 2: 0 + (1<<6) = 64 */
    {64, 9},    /* 3: 64 + (1<<9) = 576 */
    {576, 11},  /* 4: 576 + (1<<11) = 2624 */
    {2624, 13}, /* 5: 2624 + (1<<13) = 10816 */
    /* Early-stream overrides: */
    {64, 8},   /* 6: replaces 3 when pos < 320 */
    {576, 8},  /* 7: replaces 4 when pos < 832 */
    {576, 9},  /* 8: replaces 4 when pos < 1088 */
    {576, 10}, /* 9: replaces 4 when pos < 1600 */
    {2624, 8},  /* 10: replaces 5 when pos < 2880 */
    {2624, 9},  /* 11: replaces 5 when pos < 3136 */
    {2624, 10}, /* 12: replaces 5 when pos < 3648 */
    {2624, 11}, /* 13: replaces 5 when pos < 4672 */
    {2624, 12}, /* 14: replaces 5 when pos < 6720 */
};

static const variable_length_table byte_ranges[] = {
    {0, 4},   /* 0 + (1<<4) = 16 */
    {16, 4},  /* 16 + (1<<4) = 32 */
    {32, 5},  /* 32 + (1<<5) = 64 */
    {64, 6},  /* 64 + (1<<6) = 128 */
    {128, 6}, /* 128 + (1<<6) = 192 */
    {192, 6}, /* 192 + (1<<6) = 256 */
};

static const uint8_t byte_decode_trees[][5] = {
    {0x12, 0x2d, 0xef, 0x1c, 0xab}, {0x12, 0x23, 0xde, 0xab, 0xcf}, {0x12, 0x2c, 0xd2, 0xab, 0xef},
    {0x12, 0xa2, 0xd2, 0xbc, 0xef}, {0x12, 0xa2, 0xc2, 0xbd, 0xef}, {0x12, 0xa2, 0xcd, 0xb1, 0xef},
    {0x12, 0xab, 0x12, 0xcd, 0xef}, {0x12, 0xab, 0x1d, 0xc1, 0xef}, {0x12, 0xab, 0xc1, 0xd1, 0xef},
    {0xa1, 0x12, 0x2c, 0xde, 0xbf}, {0xa1, 0x1d, 0x1c, 0xb1, 0xef}, {0xa1, 0x12, 0x2d, 0xef, 0xbc},
    {0xa1, 0x12, 0xb2, 0xde, 0xcf}, {0xa1, 0x12, 0xbc, 0xd1, 0xef}, {0xa1, 0x1c, 0xb1, 0xd1, 0xef},
    {0xa1, 0xb1, 0x12, 0xcd, 0xef}, {0xa1, 0xb1, 0xc1, 0xd1, 0xef}, {0x12, 0x1c, 0xde, 0xab, 0x00},
    {0x12, 0xa2, 0xcd, 0xbe, 0x00}, {0x12, 0xab, 0xc1, 0xde, 0x00}, {0xa1, 0x1d, 0x1c, 0xbe, 0x00},
    {0xa1, 0x12, 0xbc, 0xde, 0x00}, {0xa1, 0x1c, 0xb1, 0xde, 0x00}, {0xa1, 0xb1, 0xc1, 0xde, 0x00},
    {0x1d, 0x1c, 0xab, 0x00, 0x00}, {0x1c, 0xa1, 0xbd, 0x00, 0x00}, {0x12, 0xab, 0xcd, 0x00, 0x00},
    {0xa1, 0x1c, 0xbd, 0x00, 0x00}, {0xa1, 0xb1, 0xcd, 0x00, 0x00}, {0xa1, 0xbc, 0x00, 0x00, 0x00},
    {0xab, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x00},
};

/* --- Decode helper functions --- */

static int decode_variable_length(lha_bitio *input, const variable_length_table *table, unsigned int header)
{
    int value = (int)lha_bitio_next_bits(input, (int)table[header].bits);

    return (int)table[header].offset + value;
}

static int next_bit_after_threshold(lha_bitio *input, unsigned int pos, unsigned int threshold, int def)
{
    if(pos >= threshold) return (int)lha_bitio_next_bit(input);

    return def;
}

static int read_copy_byte_count(lha_bitio *input)
{
    int x = (int)lha_bitio_next_bits(input, 2);

    if(x < 3) return x + 3;

    x = (int)lha_bitio_next_bits(input, 3);

    if(x < 5) return x + 6;
    else if(x == 5) return (int)lha_bitio_next_bits(input, 2) + 11;
    else if(x == 6) return (int)lha_bitio_next_bits(input, 3) + 15;

    /* x == 7 */
    x = (int)lha_bitio_next_bits(input, 6);

    if(x < 62) return x + 23;
    else if(x == 62) return (int)lha_bitio_next_bits(input, 5) + 85;

    /* x == 63 */
    return (int)lha_bitio_next_bits(input, 7) + 117;
}

static int read_copy_type_range(lha_bitio *input, unsigned int pos)
{
    int x = (int)lha_bitio_next_bit(input);

    if(x == 0)
    {
        x = next_bit_after_threshold(input, pos, 576, 0);

        if(x != 0) return 4;

        return next_bit_after_threshold(input, pos, 64, 0);
    }
    else
    {
        x = next_bit_after_threshold(input, pos, 64, 1);

        if(x == 0) return 3;

        x = next_bit_after_threshold(input, pos, 2624, 1);

        if(x != 0) return 2;

        return 5;
    }
}

static int read_byte_decode_index(lha_bitio *input, const uint8_t *tree)
{
    const uint8_t *ptr = tree;

    if(ptr[0] == 0) return 0;

    for(;;)
    {
        int          bit = (int)lha_bitio_next_bit(input);
        unsigned int child;

        if(bit == 0)
            child = (*ptr >> 4) & 0x0f;
        else
            child = *ptr & 0x0f;

        if(child >= 10) return (int)(child - 10);

        ptr += child;
    }
}

static int read_byte(lha_bitio *input, const uint8_t *tree, pma1_history_list *history)
{
    int index = read_byte_decode_index(input, tree);
    int count = decode_variable_length(input, byte_ranges, (unsigned int)index);

    return find_in_history_list(history, (uint8_t)count);
}

static int read_byte_block_count(lha_bitio *input)
{
    int x = (int)lha_bitio_next_bits(input, 2);

    if(x < 3) return x + 1;

    x = (int)lha_bitio_next_bits(input, 3);

    if(x < 7) return x + 4;

    x = (int)lha_bitio_next_bits(input, 4);

    if(x < 14) return x + 11;
    else if(x == 14) return (int)lha_bitio_next_bits(input, 6) + 25;

    /* x == 15 */
    return (int)lha_bitio_next_bits(input, 7) + 89;
}

/* --- Main decompression --- */

AARU_EXPORT int AARU_CALL pmarc_decompress_pm1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                size_t *out_len)
{
    lha_bitio          bitio;
    lha_lzss           lzss;
    pma1_history_list  history;
    const uint8_t     *byte_tree;
    size_t             expected;
    int                bytes_left   = 0;
    bool               next_is_match = false;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    expected = *out_len;

    if(!lha_lzss_init(&lzss, 16384, out_buf, expected)) return -1;

    lha_bitio_init(&bitio, in_buf, in_len);
    init_history_list(&history);

    /* Read 5-bit header selecting the byte decode tree */
    {
        int index = (int)lha_bitio_next_bits(&bitio, 5);
        byte_tree = byte_decode_trees[index];
    }

    while(lzss.out_pos < expected && !lha_bitio_at_eof(&bitio))
    {
        if(bytes_left > 0)
        {
            /* Emit a byte from the history-encoded stream */
            uint8_t b = (uint8_t)read_byte(&bitio, byte_tree, &history);
            lha_lzss_emit_literal(&lzss, b);
            update_history_list(&history, b);
            bytes_left--;
        }
        else if(next_is_match || lha_bitio_next_bit(&bitio) == 0)
        {
            /* Match/copy */
            int range_index, count, history_distance, offset;

            next_is_match = false;
            range_index = read_copy_type_range(&bitio, (unsigned int)lzss.position);

            if(range_index < 2)
                count = 2;
            else
                count = read_copy_byte_count(&bitio);

            /* Apply early-stream overrides */
            if(range_index == 3)
            {
                if(lzss.position < 320) range_index = 6;
            }
            else if(range_index == 4)
            {
                if(lzss.position < 832)
                    range_index = 7;
                else if(lzss.position < 1088)
                    range_index = 8;
                else if(lzss.position < 1600)
                    range_index = 9;
            }
            else if(range_index == 5)
            {
                if(lzss.position < 2880)
                    range_index = 10;
                else if(lzss.position < 3136)
                    range_index = 11;
                else if(lzss.position < 3648)
                    range_index = 12;
                else if(lzss.position < 4672)
                    range_index = 13;
                else if(lzss.position < 6720)
                    range_index = 14;
            }

            history_distance = decode_variable_length(&bitio, copy_ranges, (unsigned int)range_index);
            offset           = history_distance + 1;

            lha_lzss_emit_match(&lzss, offset, count);

            /* Update history for all emitted match bytes */
            {
                size_t match_start = lzss.out_pos - (size_t)count;
                int    i;

                for(i = 0; i < count; i++) update_history_list(&history, out_buf[match_start + i]);
            }
        }
        else
        {
            /* Byte block: read count, then emit that many literal bytes */
            bytes_left = read_byte_block_count(&bitio);

            if(bytes_left < MAX_BYTE_BLOCK_LEN) next_is_match = true;

            /* Process first byte immediately */
            {
                uint8_t b = (uint8_t)read_byte(&bitio, byte_tree, &history);
                lha_lzss_emit_literal(&lzss, b);
                update_history_list(&history, b);
                bytes_left--;
            }
        }
    }

    *out_len = lzss.out_pos;
    lha_lzss_cleanup(&lzss);
    return 0;
}
