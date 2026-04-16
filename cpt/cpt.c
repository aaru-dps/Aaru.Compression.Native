/*
 * cpt.c - Compact Pro archive decompression
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

#include "cpt.h"

#include <stdlib.h>
#include <string.h>

#include "../pak/bitstream.h"
#include "../pak/prefixcode.h"

/* ============ Constants ============ */

#define CPT_WINDOW_SIZE  8192
#define CPT_BLOCK_SIZE   0x1fff0
#define CPT_RLE_ESCAPE   0x81
#define CPT_LITERAL_SYMS 256
#define CPT_LENGTH_SYMS  64
#define CPT_OFFSET_SYMS  128
#define CPT_MAX_CODELEN  15
#define CPT_OFFSET_LOW   6

/* ============ Bitstream helpers ============ */

/** Discard remaining bits in the current byte to align to a byte boundary. */
static void bs_align_to_byte(BitStream *bs)
{
    int remainder = bs->bitcount & 7;

    if(remainder > 0)
    {
        bs->bitbuffer <<= remainder;
        bs->bitcount -= remainder;
    }
}

/** Return the logical byte offset of consumed data (bytes read from source). */
static size_t bs_byte_offset(const BitStream *bs) { return bs->pos - (size_t)((bs->bitcount + 7) / 8); }

/** Skip n bytes through the bit buffer (must be at a byte boundary). */
static void bs_skip_bytes(BitStream *bs, int n)
{
    for(int i = 0; i < n; i++) bitstream_read_bits(bs, 8);
}

/* ============ Huffman table reading ============ */

/**
 * Read a canonical Huffman code table from the bitstream.
 *
 * Format: one byte giving the number of packed byte pairs, then that many
 * bytes each containing two 4-bit code lengths (high nibble first).
 * Maximum code length is 15. Uses shortest-code-is-zeros convention.
 *
 * @param bs   Bitstream positioned at the start of the code table.
 * @param size Number of symbols in the code alphabet.
 * @return Allocated PrefixCode on success, NULL on error.
 */
static PrefixCode *cpt_read_code_table(BitStream *bs, int size)
{
    int numbytes = (int)bitstream_read_bits(bs, 8);

    if(numbytes * 2 > size) return NULL;

    int *lengths = (int *)calloc((size_t)size, sizeof(int));

    if(!lengths) return NULL;

    for(int i = 0; i < numbytes; i++)
    {
        int val            = (int)bitstream_read_bits(bs, 8);
        lengths[2 * i]     = val >> 4;
        lengths[2 * i + 1] = val & 0x0f;
    }

    /* Remaining symbols have length 0 (no code assigned). */
    PrefixCode *code = prefix_code_alloc_with_lengths(lengths, size, CPT_MAX_CODELEN, true);
    free(lengths);
    return code;
}

/* ============ LZH decoder ============ */

/**
 * Decompress LZH-encoded data (block-based Huffman LZSS, 8 KB window).
 *
 * The output is the intermediate RLE-encoded stream that must be further
 * processed by the RLE decoder to produce the final data.
 *
 * @param dst       Output buffer.
 * @param dst_cap   Capacity of output buffer.
 * @param src       Input compressed data.
 * @param src_size  Size of input data.
 * @param out_len   Receives the number of bytes actually written.
 * @return 0 on success, -1 on error.
 */
static int cpt_lzh_decode(uint8_t *dst, size_t dst_cap, const uint8_t *src, size_t src_size, size_t *out_len)
{
    BitStream bs;
    bitstream_init(&bs, src, src_size);

    uint8_t window[CPT_WINDOW_SIZE];
    memset(window, 0, sizeof(window));
    size_t win_pos = 0;

    size_t out_pos     = 0;
    int    block_cost  = CPT_BLOCK_SIZE; /* Force first block read immediately. */
    size_t block_start = 0;

    PrefixCode *literal_code = NULL;
    PrefixCode *length_code  = NULL;
    PrefixCode *offset_code  = NULL;

    while(!bitstream_eof(&bs))
    {
        /* New block required? */
        if(block_cost >= CPT_BLOCK_SIZE)
        {
            if(block_start)
            {
                /* Inter-block alignment padding:
                 * Align to byte boundary, then skip 2 or 3 bytes depending on
                 * whether the consumed byte count since block_start is odd or even. */
                bs_align_to_byte(&bs);

                size_t consumed = bs_byte_offset(&bs) - block_start;

                if(consumed & 1)
                    bs_skip_bytes(&bs, 3);
                else
                    bs_skip_bytes(&bs, 2);
            }

            /* Free previous tables. */
            prefix_code_free(literal_code);
            prefix_code_free(length_code);
            prefix_code_free(offset_code);
            literal_code = length_code = offset_code = NULL;

            /* Read three Huffman code tables for this block. */
            literal_code = cpt_read_code_table(&bs, CPT_LITERAL_SYMS);
            length_code  = cpt_read_code_table(&bs, CPT_LENGTH_SYMS);
            offset_code  = cpt_read_code_table(&bs, CPT_OFFSET_SYMS);

            if(!literal_code || !length_code || !offset_code) goto fail;

            block_cost  = 0;
            block_start = bs_byte_offset(&bs);
        }

        if(bitstream_eof(&bs)) break;

        /* Read next symbol: bit=1 means literal, bit=0 means match. */
        uint32_t flag = bitstream_read_bit(&bs);

        if(flag)
        {
            /* Literal byte. */
            int sym = prefix_code_read_symbol(&bs, literal_code);

            if(sym < 0) goto fail;
            if(out_pos >= dst_cap) goto fail;

            uint8_t byte    = (uint8_t)sym;
            dst[out_pos++]  = byte;
            window[win_pos] = byte;
            win_pos         = (win_pos + 1) & (CPT_WINDOW_SIZE - 1);
            block_cost += 2;
        }
        else
        {
            /* LZSS match. */
            int match_len = prefix_code_read_symbol(&bs, length_code);

            if(match_len < 0) goto fail;

            int offset_high = prefix_code_read_symbol(&bs, offset_code);

            if(offset_high < 0) goto fail;

            int offset = (offset_high << CPT_OFFSET_LOW) | (int)bitstream_read_bits(&bs, CPT_OFFSET_LOW);

            /* Copy from window. */
            size_t copy_pos = (win_pos - offset) & (CPT_WINDOW_SIZE - 1);

            for(int i = 0; i < match_len; i++)
            {
                if(out_pos >= dst_cap) goto fail;

                uint8_t byte    = window[copy_pos];
                dst[out_pos++]  = byte;
                window[win_pos] = byte;
                win_pos         = (win_pos + 1) & (CPT_WINDOW_SIZE - 1);
                copy_pos        = (copy_pos + 1) & (CPT_WINDOW_SIZE - 1);
            }

            block_cost += 3;
        }
    }

    *out_len = out_pos;

    prefix_code_free(literal_code);
    prefix_code_free(length_code);
    prefix_code_free(offset_code);
    return 0;

fail:
    prefix_code_free(literal_code);
    prefix_code_free(length_code);
    prefix_code_free(offset_code);
    return -1;
}

/* ============ RLE decoder ============ */

int cpt_rle_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    size_t dst_cap = *dst_size;
    size_t out_pos = 0;
    size_t in_pos  = 0;

    uint8_t saved       = 0;
    int     repeat      = 0;
    int     halfescaped = 0;

    while(out_pos < dst_cap)
    {
        /* Emit pending repeats first. */
        if(repeat > 0)
        {
            repeat--;
            if(out_pos >= dst_cap) break;

            dst_buffer[out_pos++] = saved;
            continue;
        }

        int byte;

        if(halfescaped)
        {
            byte        = CPT_RLE_ESCAPE;
            halfescaped = 0;
        }
        else
        {
            if(in_pos >= src_size) break;

            byte = src_buffer[in_pos++];
        }

        if(byte == CPT_RLE_ESCAPE)
        {
            if(in_pos >= src_size) break;

            byte = src_buffer[in_pos++];

            if(byte == 0x82)
            {
                if(in_pos >= src_size) break;

                byte = src_buffer[in_pos++];

                if(byte != 0)
                {
                    /* Repeat previous byte: byte-2 additional copies. */
                    repeat = byte - 2;

                    if(out_pos >= dst_cap) break;

                    dst_buffer[out_pos++] = saved;
                }
                else
                {
                    /* Literal 0x81 0x82 sequence. */
                    repeat = 1;
                    saved  = 0x82;

                    if(out_pos >= dst_cap) break;

                    dst_buffer[out_pos++] = CPT_RLE_ESCAPE;
                }
            }
            else if(byte == CPT_RLE_ESCAPE)
            {
                /* Escaped 0x81: output 0x81 and set half-escaped state. */
                halfescaped = 1;
                saved       = CPT_RLE_ESCAPE;

                if(out_pos >= dst_cap) break;

                dst_buffer[out_pos++] = saved;
            }
            else
            {
                /* Literal 0x81 followed by this byte. */
                repeat = 1;
                saved  = (uint8_t)byte;

                if(out_pos >= dst_cap) break;

                dst_buffer[out_pos++] = CPT_RLE_ESCAPE;
            }
        }
        else
        {
            saved = (uint8_t)byte;

            if(out_pos >= dst_cap) break;

            dst_buffer[out_pos++] = saved;
        }
    }

    *dst_size = out_pos;
    return 0;
}

/* ============ Combined LZH + RLE decoder ============ */

int cpt_lzh_rle_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    size_t final_size = *dst_size;

    /* Intermediate buffer for LZH output (RLE-encoded data).
     * The RLE encoding can at most double the data size (every 0x81 byte in
     * the original needs escaping), so 2 × final_size is a safe upper bound. */
    size_t   intermediate_cap = final_size * 2 + 4096;
    uint8_t *intermediate     = (uint8_t *)malloc(intermediate_cap);

    if(!intermediate) return -1;

    size_t intermediate_len = 0;
    int    ret              = cpt_lzh_decode(intermediate, intermediate_cap, src_buffer, src_size, &intermediate_len);

    if(ret != 0)
    {
        free(intermediate);
        return -1;
    }

    /* Now RLE-decode the intermediate data to produce the final output. */
    *dst_size = final_size;
    ret       = cpt_rle_decode_buffer(dst_buffer, dst_size, intermediate, intermediate_len);

    free(intermediate);
    return ret;
}
