/*
 * dd.c - DiskDoubler archive decompression
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

#include "dd.h"

#include <stdlib.h>
#include <string.h>

#include "../cpt/cpt.h"
#include "../pak/bitstream.h"
#include "../pak/prefixcode.h"

/* ================================================================
 * ADn decompressor (methods 6 and 9):
 * Block-based LZSS with 8 KB output blocks.
 *
 * Each block has a 12-byte header:
 *   bytes 0-1:  compressed block size (BE u16)
 *   bytes 2-3:  uncompressed block size (BE u16, max 0x2000)
 *   bytes 4-7:  reserved (XOR accumulator)
 *   byte 8:     data XOR checksum
 *   byte 9:     flags (bit 0: 1=uncompressed)
 *   byte 10:    padding
 *   byte 11:    header XOR checksum
 *
 * LZSS encoding (MSB-first bits):
 *   bit=0: literal (8 bits)
 *   bit=1: match
 *     far flag (1 bit): 1 = 12-bit offset, 0 = 8-bit offset
 *     offset (8 or 12 bits)
 *     length: variable encoding:
 *       0 -> 2
 *       10 -> 3 or 4 (1 more bit)
 *       11 -> read 4 bits + 5
 * ================================================================ */

#define ADN_BLOCK_SIZE 0x2000

/** Read a big-endian uint16 from raw data. */
static uint16_t read_be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

/** Read a big-endian uint32 from raw data. */
static uint32_t read_be32(const uint8_t *p)
{ return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

/** Copy bytes allowing overlapping source/destination (byte-repeat). */
static void copy_bytes_repeat(uint8_t *dst, const uint8_t *src, int length)
{
    for(int i = 0; i < length; i++) dst[i] = src[i];
}

int dd_adn_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    const size_t dst_cap = *dst_size;
    size_t       out_pos = 0;
    size_t       in_pos  = 0;

    while(in_pos < src_size && out_pos < dst_cap)
    {
        /* Need at least 12 bytes for the block header. */
        if(in_pos + 12 > src_size) return -1;

        /* Parse block header. */
        uint8_t headxor = 0;

        uint16_t compsize = read_be16(&src_buffer[in_pos]);
        headxor ^= (uint8_t)(compsize >> 8) ^ (uint8_t)compsize;

        uint16_t uncompsize = read_be16(&src_buffer[in_pos + 2]);
        if(uncompsize > ADN_BLOCK_SIZE) return -1;
        headxor ^= (uint8_t)(uncompsize >> 8) ^ (uint8_t)uncompsize;

        /* 4 reserved bytes contribute to header XOR. */
        headxor ^= src_buffer[in_pos + 4];
        headxor ^= src_buffer[in_pos + 5];
        headxor ^= src_buffer[in_pos + 6];
        headxor ^= src_buffer[in_pos + 7];

        uint8_t datacorrectxor = src_buffer[in_pos + 8];
        headxor ^= datacorrectxor;

        uint8_t flags = src_buffer[in_pos + 9];
        headxor ^= flags;

        headxor ^= src_buffer[in_pos + 10]; /* padding */

        uint8_t headcorrectxor = src_buffer[in_pos + 11];
        if(headxor != headcorrectxor) return -1;

        size_t block_data_start = in_pos + 12;
        size_t next_block       = block_data_start + compsize;

        if(next_block > src_size) return -1;
        if(out_pos + uncompsize > dst_cap) return -1;

        uint8_t *outbuf = &dst_buffer[out_pos];

        if(flags & 1)
        {
            /* Uncompressed block: copy raw bytes. */
            memcpy(outbuf, &src_buffer[block_data_start], uncompsize);
        }
        else
        {
            /* Compressed block: LZSS decode. */
            BitStream bs;
            bitstream_init(&bs, &src_buffer[block_data_start], compsize);

            int currpos = 0;
            while(currpos < uncompsize)
            {
                uint32_t ismatch = bitstream_read_bit(&bs);

                if(!ismatch) { outbuf[currpos++] = (uint8_t)bitstream_read_bits(&bs, 8); }
                else
                {
                    uint32_t isfar  = bitstream_read_bit(&bs);
                    int      offset = (int)bitstream_read_bits(&bs, isfar ? 12 : 8);

                    if(offset > currpos) return -1;

                    int length;
                    if(bitstream_read_bit(&bs) == 0) { length = 2; }
                    else
                    {
                        if(bitstream_read_bit(&bs) == 0)
                        {
                            if(bitstream_read_bit(&bs) == 0)
                                length = 3;
                            else
                                length = 4;
                        }
                        else
                        {
                            length = (int)bitstream_read_bits(&bs, 4) + 5;
                        }
                    }

                    if(currpos + length > uncompsize) length = uncompsize - currpos;
                    if(length > offset) return -1;

                    copy_bytes_repeat(&outbuf[currpos], &outbuf[currpos - offset], length);
                    currpos += length;
                }
            }
        }

        out_pos += uncompsize;
        in_pos = next_block;
    }

    *dst_size = out_pos;
    return 0;
}

/* ================================================================
 * DDn decompressor (method 10):
 * Block-based Huffman LZ77 with 64 KB sliding window.
 *
 * Each block has a 22-byte header followed by Huffman tables and
 * compressed data. The block is organized in three sections:
 *   1. Offset codes (Huffman-coded, decoded to uint16 offsets)
 *   2. Literal bytes (raw or Huffman-coded)
 *   3. Length codes (Huffman-coded, interleaved decode)
 *
 * Block header (22 bytes):
 *   bytes 0-3:   uncompressed block size (BE u32)
 *   bytes 4-5:   number of literals (BE u16)
 *   bytes 6-7:   number of match offsets (BE u16)
 *   bytes 8-9:   length code compressed size (BE u16)
 *   bytes 10-11: literal compressed size (BE u16)
 *   bytes 12-13: offset code compressed size (BE u16)
 *   byte 14:     flags (bit 6: uncompressed block, bit 7: literals Huffman)
 *   byte 15:     padding
 *   bytes 16-18: data XOR checksum bytes
 *   byte 19:     uncompressed data XOR checksum
 *   byte 20:     padding
 *   byte 21:     header XOR checksum
 *
 * Huffman code table header (4-byte packed word):
 *   bits 31-24: numcodes - 1
 *   bits 23-13: compressed size in bytes
 *   bits 12-8:  maximum code length
 *   bits 7-3:   bits per code length field
 *   bit 2:      uses zero coding (sparse)
 *   bits 1-0:   reserved
 *
 * Offset decoding:
 *   slot < 4:  offset = slot + 1
 *   slot >= 4: bits = slot/2 - 1; base = ((2 + (slot & 1)) << bits) + 1;
 *              offset = base + read_bits(bits)
 *
 * Length code interpretation:
 *   code == 0:    output one literal
 *   1 <= code < 128: LZ77 match, length = code + 2, offset from offset table
 *   code >= 128:  run of (1 << (code - 128)) literals, first one output
 *                 immediately, rest queued
 * ================================================================ */

#define DDN_WINDOW_SIZE 65536
#define DDN_WINDOW_MASK (DDN_WINDOW_SIZE - 1)
#define DDN_BUFFER_SIZE 0x10000

/**
 * Read a canonical Huffman code table from the DDn bitstream.
 *
 * The 4-byte header packs: numcodes, compressed byte size, max code length,
 * bits-per-length, and a zero-coding flag.
 *
 * Returns an allocated PrefixCode on success, NULL on error.
 * The caller must free the returned code with prefix_code_free().
 */
static PrefixCode *ddn_read_code(BitStream *bs)
{
    uint32_t head = bitstream_read_bits(bs, 32);

    int numcodes  = ((head >> 24) & 0xff) + 1;
    int numbytes  = (head >> 13) & 0x7ff;
    int maxlength = (head >> 8) & 0x1f;
    int numbits   = (head >> 3) & 0x1f;

    if(numcodes <= 0 || maxlength <= 0 || numbits <= 0) return NULL;

    /* Remember where we started reading code data so we can skip to the end. */
    /* Compute byte offset: pos is the next byte, bitcount is bits remaining. */
    size_t byte_start   = bs->pos - (size_t)((bs->bitcount + 7) / 8);
    /* But we also need to account for bits already consumed within the
     * bitstream up to this point. We track the logical end position. */
    size_t end_byte_pos = byte_start + (size_t)numbytes;

    int *codelengths = (int *)calloc((size_t)numcodes, sizeof(int));
    if(!codelengths) return NULL;

    if(head & 0x04)
    {
        /* Zero coding: each entry has a 1-bit flag, only present symbols
         * have a numbits-wide code length following the flag. */
        for(int i = 0; i < numcodes; i++)
        {
            if(bitstream_read_bit(bs))
            {
                codelengths[i] = (int)bitstream_read_bits(bs, numbits);
                if(codelengths[i] > maxlength)
                {
                    free(codelengths);
                    return NULL;
                }
            }
            else
            {
                codelengths[i] = 0;
            }
        }
    }
    else
    {
        /* Dense coding: each entry is numbits wide. */
        for(int i = 0; i < numcodes; i++)
        {
            codelengths[i] = (int)bitstream_read_bits(bs, numbits);
            if(codelengths[i] > maxlength)
            {
                free(codelengths);
                return NULL;
            }
        }
    }

    /* Seek past the code table data to the byte-aligned end position. */
    /* Reset bitstream to end_byte_pos. */
    bs->pos       = end_byte_pos;
    bs->bitbuffer = 0;
    bs->bitcount  = 0;

    PrefixCode *code = prefix_code_alloc_with_lengths(codelengths, numcodes, maxlength, true);
    free(codelengths);
    return code;
}

int dd_ddn_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    const size_t dst_cap = *dst_size;
    size_t       out_pos = 0;

    /* 64 KB sliding window for LZ77 match references. */
    uint8_t *window = (uint8_t *)calloc(DDN_WINDOW_SIZE, 1);
    if(!window) return -1;
    size_t win_pos = 0;

    /* Temporary buffer for interleaved literal/offset data. */
    uint8_t *buffer = (uint8_t *)malloc(DDN_BUFFER_SIZE);
    if(!buffer)
    {
        free(window);
        return -1;
    }

    /* Block iterator: tracks input position via a raw pointer. */
    size_t next_block = 0;

    int result = 0;

    while(next_block < src_size && out_pos < dst_cap)
    {
        /* Need at least 22 bytes for the block header. */
        if(next_block + 22 > src_size)
        {
            result = -1;
            break;
        }

        const uint8_t *hdr     = &src_buffer[next_block];
        uint8_t        headxor = 0;

        uint32_t uncompsize = read_be32(&hdr[0]);
        headxor ^= (uint8_t)(uncompsize >> 24) ^ (uint8_t)(uncompsize >> 16) ^ (uint8_t)(uncompsize >> 8) ^
                   (uint8_t)uncompsize;

        uint16_t numliterals = read_be16(&hdr[4]);
        headxor ^= (uint8_t)(numliterals >> 8) ^ (uint8_t)numliterals;

        uint16_t numoffsets = read_be16(&hdr[6]);
        headxor ^= (uint8_t)(numoffsets >> 8) ^ (uint8_t)numoffsets;

        uint16_t lengthcompsize = read_be16(&hdr[8]);
        headxor ^= (uint8_t)(lengthcompsize >> 8) ^ (uint8_t)lengthcompsize;

        uint16_t literalcompsize = read_be16(&hdr[10]);
        headxor ^= (uint8_t)(literalcompsize >> 8) ^ (uint8_t)literalcompsize;

        uint16_t offsetcompsize = read_be16(&hdr[12]);
        headxor ^= (uint8_t)(offsetcompsize >> 8) ^ (uint8_t)offsetcompsize;

        uint8_t flags = hdr[14];
        headxor ^= flags;

        headxor ^= hdr[15]; /* padding */

        uint8_t datacorrectxor1 = hdr[16];
        headxor ^= datacorrectxor1;
        uint8_t datacorrectxor2 = hdr[17];
        headxor ^= datacorrectxor2;
        uint8_t datacorrectxor3 = hdr[18];
        headxor ^= datacorrectxor3;

        uint8_t uncompcorrectxor = hdr[19];
        headxor ^= uncompcorrectxor;

        headxor ^= hdr[20]; /* padding */

        uint8_t headcorrectxor = hdr[21];
        if(headxor != headcorrectxor)
        {
            result = -1;
            break;
        }

        if(out_pos + uncompsize > dst_cap)
        {
            result = -1;
            break;
        }

        size_t block_data_start = next_block + 22;

        if(flags & 0x40)
        {
            /* Uncompressed block. */
            BitStream bs;
            bitstream_init(&bs, &src_buffer[block_data_start], src_size - block_data_start);

            for(uint32_t i = 0; i < uncompsize; i++)
            {
                uint8_t byte        = (uint8_t)bitstream_read_bits(&bs, 8);
                dst_buffer[out_pos] = byte;
                window[win_pos]     = byte;
                win_pos             = (win_pos + 1) & DDN_WINDOW_MASK;
                out_pos++;
            }

            /* Advance past the raw data. */
            next_block = block_data_start + uncompsize;
            continue;
        }

        /* Validate buffer sizes. */
        if((size_t)numliterals + (size_t)numoffsets * 2 > DDN_BUFFER_SIZE)
        {
            result = -1;
            break;
        }

        /* Set up pointers into the temporary buffer. */
        uint8_t  *literalptr = buffer;
        uint16_t *offsetptr  = (uint16_t *)&buffer[numliterals];

        /* Compute section boundaries.
         * After header: offset codes, then literals, then length codes. */
        size_t literal_start = block_data_start + offsetcompsize;
        size_t length_start  = literal_start + literalcompsize;
        next_block           = length_start + lengthcompsize;

        if(next_block > src_size)
        {
            result = -1;
            break;
        }

        /* -- Phase 1: Decode offset table -- */
        BitStream bs;
        bitstream_init(&bs, &src_buffer[block_data_start], offsetcompsize);

        PrefixCode *offsetcode = ddn_read_code(&bs);
        if(!offsetcode)
        {
            result = -1;
            break;
        }

        for(int i = 0; i < numoffsets; i++)
        {
            int slot = prefix_code_read_symbol(&bs, offsetcode);
            if(slot < 0)
            {
                prefix_code_free(offsetcode);
                result = -1;
                goto done;
            }

            if(slot < 4) { offsetptr[i] = (uint16_t)(slot + 1); }
            else
            {
                int bits     = slot / 2 - 1;
                int start    = ((2 + (slot & 1)) << bits) + 1;
                offsetptr[i] = (uint16_t)(start + (int)bitstream_read_bits(&bs, bits));
            }
        }

        prefix_code_free(offsetcode);
        offsetcode = NULL;

        /* -- Phase 2: Decode literal data -- */
        BitStream lbs;
        bitstream_init(&lbs, &src_buffer[literal_start], literalcompsize);

        if(flags & 0x80)
        {
            /* Huffman-compressed literals. */
            PrefixCode *literalcode = ddn_read_code(&lbs);
            if(!literalcode)
            {
                result = -1;
                break;
            }

            for(int i = 0; i < numliterals; i++)
            {
                int sym = prefix_code_read_symbol(&lbs, literalcode);
                if(sym < 0)
                {
                    prefix_code_free(literalcode);
                    result = -1;
                    goto done;
                }
                literalptr[i] = (uint8_t)sym;
            }

            prefix_code_free(literalcode);
        }
        else
        {
            /* Uncompressed literals: read raw bytes. */
            for(int i = 0; i < numliterals; i++) literalptr[i] = (uint8_t)bitstream_read_bits(&lbs, 8);
        }

        /* -- Phase 3: Decode length codes and produce output -- */
        BitStream lenbs;
        bitstream_init(&lenbs, &src_buffer[length_start], lengthcompsize);

        PrefixCode *lengthcode = ddn_read_code(&lenbs);
        if(!lengthcode)
        {
            result = -1;
            break;
        }

        uint8_t  *lit_cursor = literalptr;
        uint16_t *off_cursor = offsetptr;
        int       lits_left  = 0;

        size_t block_end = out_pos + uncompsize;

        while(out_pos < block_end)
        {
            if(lits_left > 0)
            {
                /* Pending run of literals from a previous code >= 128. */
                uint8_t byte        = *lit_cursor++;
                dst_buffer[out_pos] = byte;
                window[win_pos]     = byte;
                win_pos             = (win_pos + 1) & DDN_WINDOW_MASK;
                out_pos++;
                lits_left--;
                continue;
            }

            int code = prefix_code_read_symbol(&lenbs, lengthcode);
            if(code < 0)
            {
                prefix_code_free(lengthcode);
                result = -1;
                goto done;
            }

            if(code == 0)
            {
                /* Single literal byte. */
                uint8_t byte        = *lit_cursor++;
                dst_buffer[out_pos] = byte;
                window[win_pos]     = byte;
                win_pos             = (win_pos + 1) & DDN_WINDOW_MASK;
                out_pos++;
            }
            else if(code < 128)
            {
                /* LZ77 match: length = code + 2, offset from table. */
                int length = code + 2;
                int offset = *off_cursor++;

                if((size_t)offset > win_pos + out_pos)
                {
                    prefix_code_free(lengthcode);
                    result = -1;
                    goto done;
                }

                /* Cap length at block boundary. */
                if(out_pos + (size_t)length > block_end) length = (int)(block_end - out_pos);

                for(int i = 0; i < length; i++)
                {
                    uint8_t byte        = window[(win_pos - (size_t)offset) & DDN_WINDOW_MASK];
                    dst_buffer[out_pos] = byte;
                    window[win_pos]     = byte;
                    win_pos             = (win_pos + 1) & DDN_WINDOW_MASK;
                    out_pos++;
                }
            }
            else
            {
                /* Run of consecutive literals: 1 << (code - 128) total.
                 * Output the first one now, queue the rest. */
                int run_length = 1 << (code - 128);

                /* Cap at block boundary. */
                if(out_pos + (size_t)run_length > block_end) run_length = (int)(block_end - out_pos);

                uint8_t byte        = *lit_cursor++;
                dst_buffer[out_pos] = byte;
                window[win_pos]     = byte;
                win_pos             = (win_pos + 1) & DDN_WINDOW_MASK;
                out_pos++;
                lits_left = run_length - 1;
            }
        }

        prefix_code_free(lengthcode);
        lengthcode = NULL;
    }

done:
    free(window);
    free(buffer);
    *dst_size = out_pos;
    return result;
}

/* ================================================================
 * Method 2 decompressor (also used by method 5):
 * Adaptive Huffman coding using splay trees.
 *
 * The decoder maintains an array of binary trees (one per tree index).
 * Each tree has 256 internal nodes (indices 0-255) and 256 leaf nodes
 * (indices 256-511, representing byte values 0-255).
 *
 * To decode a byte, walk from node 1 (root) using input bits:
 *   bit 0 -> left child, bit 1 -> right child.
 * When a leaf (node >= 256) is reached, the byte value is (node - 256).
 *
 * After each decoded byte, the tree is restructured ("splayed") to move
 * the decoded symbol closer to the root. The next tree is selected as
 * (decoded_byte % num_trees).
 *
 * Initial state: each internal node j has left child = 2*j and right
 * child = 2*j+1, with parent pointers set symmetrically.
 * ================================================================ */

typedef struct
{
    uint8_t  parents[512];
    uint16_t leftchildren[256];
    uint16_t rightchildren[256];
} SplayTree;

static void splay_tree_init(SplayTree *tree)
{
    for(int j = 0; j < 256; j++)
    {
        tree->parents[2 * j]     = (uint8_t)j;
        tree->parents[2 * j + 1] = (uint8_t)j;
        tree->leftchildren[j]    = (uint16_t)(j * 2);
        tree->rightchildren[j]   = (uint16_t)(j * 2 + 1);
    }
}

static void splay_tree_update(SplayTree *tree, int byte)
{
    uint8_t  *parents       = tree->parents;
    uint16_t *leftchildren  = tree->leftchildren;
    uint16_t *rightchildren = tree->rightchildren;

    int node = byte + 0x100;
    for(;;)
    {
        int parentnode = parents[node];
        if(parentnode == 1) break;

        int grandparent = parents[parentnode];

        int uncle = leftchildren[grandparent];
        if(uncle == parentnode)
        {
            uncle                      = rightchildren[grandparent];
            rightchildren[grandparent] = (uint16_t)node;
        }
        else
        {
            leftchildren[grandparent] = (uint16_t)node;
        }

        if(leftchildren[parentnode] != node)
            rightchildren[parentnode] = (uint16_t)uncle;
        else
            leftchildren[parentnode] = (uint16_t)uncle;

        parents[node]  = (uint8_t)grandparent;
        parents[uncle] = (uint8_t)parentnode;

        node = grandparent;
        if(node == 1) break;
    }
}

int dd_method2_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size,
                             int num_trees)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;
    if(num_trees < 1 || num_trees > 256) return -1;

    const size_t dst_cap = *dst_size;

    SplayTree *trees = (SplayTree *)malloc((size_t)num_trees * sizeof(SplayTree));
    if(!trees) return -1;

    for(int i = 0; i < num_trees; i++) splay_tree_init(&trees[i]);

    BitStream bs;
    bitstream_init(&bs, src_buffer, src_size);

    int    currtree = 0;
    size_t out_pos  = 0;

    while(out_pos < dst_cap && !bitstream_eof(&bs))
    {
        /* Decode one byte by walking the current tree from root (node 1). */
        int node = 1;
        for(;;)
        {
            uint32_t bit = bitstream_read_bit(&bs);

            if(bit)
                node = trees[currtree].rightchildren[node];
            else
                node = trees[currtree].leftchildren[node];

            if(node >= 0x100)
            {
                int byte = node - 0x100;

                dst_buffer[out_pos++] = (uint8_t)byte;

                /* Splay the tree to move this symbol closer to root. */
                splay_tree_update(&trees[currtree], byte);

                /* Select next tree based on the decoded byte. */
                currtree = byte % num_trees;
                break;
            }
        }
    }

    free(trees);
    *dst_size = out_pos;
    return 0;
}

/* ================================================================
 * Stac LZS decompressor (method 7):
 * LZSS with 2 KB sliding window. Match lengths are unbounded.
 *
 * Encoding (MSB-first bits):
 *   bit=0: literal (8 bits)
 *   bit=1: match
 *     bit=1: 7-bit offset (short, 1-127)
 *     bit=0: 7-bit high offset + 4-bit low offset = 11-bit offset
 *            if high == 0: end of stream marker
 *   Length codes (fixed Huffman):
 *     00 -> 2,  01 -> 3,  10 -> 4
 *     1100 -> 5,  1101 -> 6,  1110 -> 7,  1111 -> 8
 *     If length == 8, read extension nibbles: add each 4-bit value
 *     until a value != 15 is read.
 * ================================================================ */

#define STAC_WINDOW_SIZE 2048
#define STAC_WINDOW_MASK (STAC_WINDOW_SIZE - 1)

/**
 * Decode a match length from the bitstream using the fixed Huffman table.
 *
 *  00 -> 2, 01 -> 3, 10 -> 4, 1100 -> 5, 1101 -> 6, 1110 -> 7, 1111 -> 8
 *  If the result is 8, extension nibbles are read until one is < 15.
 */
static int stac_read_length(BitStream *bs)
{
    int length;

    uint32_t b0 = bitstream_read_bit(bs);
    if(b0 == 0)
    {
        /* 0x -> 2 or 3 */
        length = (int)bitstream_read_bit(bs) + 2;
    }
    else
    {
        uint32_t b1 = bitstream_read_bit(bs);
        if(b1 == 0)
        {
            /* 10 -> 4 */
            length = 4;
        }
        else
        {
            /* 11xx -> 5-8 */
            length = (int)bitstream_read_bits(bs, 2) + 5;
        }
    }

    if(length == 8)
    {
        /* Extended length: keep reading 4-bit nibbles. */
        for(;;)
        {
            int code = (int)bitstream_read_bits(bs, 4);
            length += code;
            if(code != 15) break;
        }
    }

    return length;
}

int dd_stac_lzs_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    const size_t dst_cap = *dst_size;

    uint8_t window[STAC_WINDOW_SIZE];
    memset(window, 0, sizeof(window));
    size_t win_pos = 0;
    size_t out_pos = 0;

    BitStream bs;
    bitstream_init(&bs, src_buffer, src_size);

    while(out_pos < dst_cap && !bitstream_eof(&bs))
    {
        if(bitstream_read_bit(&bs) == 0)
        {
            /* Literal byte. */
            uint8_t byte          = (uint8_t)bitstream_read_bits(&bs, 8);
            dst_buffer[out_pos++] = byte;
            window[win_pos]       = byte;
            win_pos               = (win_pos + 1) & STAC_WINDOW_MASK;
        }
        else
        {
            /* Match. */
            int offset;

            if(bitstream_read_bit(&bs) == 1)
            {
                /* Short offset: 7 bits. */
                offset = (int)bitstream_read_bits(&bs, 7);
            }
            else
            {
                /* Long offset: 7-bit high + 4-bit low = 11 bits. */
                int offset_high = (int)bitstream_read_bits(&bs, 7);
                if(offset_high == 0)
                {
                    /* End of stream marker. */
                    break;
                }
                offset = (offset_high << 4) | (int)bitstream_read_bits(&bs, 4);
            }

            if((size_t)offset > out_pos) return -1;

            int length = stac_read_length(&bs);

            /* Copy from window, handling lengths > window size by chunking. */
            for(int i = 0; i < length; i++)
            {
                if(out_pos >= dst_cap) break;

                uint8_t byte          = window[(win_pos - (size_t)offset) & STAC_WINDOW_MASK];
                dst_buffer[out_pos++] = byte;
                window[win_pos]       = byte;
                win_pos               = (win_pos + 1) & STAC_WINDOW_MASK;
            }
        }
    }

    *dst_size = out_pos;
    return 0;
}

/* ================================================================
 * Compact Pro wrapper (method 8):
 * Reads a 16-byte header. If the sum of those bytes is zero, the data
 * is LZH + RLE compressed; otherwise it is RLE-only.
 * ================================================================ */

int dd_cpt_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;
    if(src_size < 16) return -1;

    int sum = 0;
    for(int i = 0; i < 16; i++) sum += src_buffer[i];

    const uint8_t *payload      = src_buffer + 16;
    size_t         payload_size = src_size - 16;

    if(sum == 0)
        return cpt_lzh_rle_decode_buffer(dst_buffer, dst_size, payload, payload_size);
    else
        return cpt_rle_decode_buffer(dst_buffer, dst_size, payload, payload_size);
}

/* ================================================================
 * LZW decompressor (method 1, Unix compress variant):
 * Variable-width LZW with LSB-first bit reading.
 *
 * flags byte: bits 0-4 = max code bits (9-16)
 *             bit 7    = block mode (code 256 triggers table clear)
 *
 * Dictionary starts with 256 single-byte entries (+ 1 reserved if
 * block mode). Code width begins at 9 and grows when the dictionary
 * size reaches a power of two. In block mode, code 256 clears the
 * table and resets code width to 9, with padding to the next code-
 * width-aligned boundary.
 * ================================================================ */

#define LZW_INITIAL_BITS 9

typedef struct
{
    uint8_t byte;
    int     parent;
} LzwNode;

int dd_lzw_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size, int flags)
{
    if(!dst_buffer || !dst_size || !src_buffer) return -1;

    int maxbits    = flags & 0x1f;
    int blockmode  = (flags & 0x80) != 0;
    int maxsymbols = 1 << maxbits;

    if(maxbits < LZW_INITIAL_BITS || maxbits > 16) return -1;

    const size_t dst_cap = *dst_size;

    /* Allocate dictionary. */
    LzwNode *nodes = (LzwNode *)malloc((size_t)maxsymbols * sizeof(LzwNode));
    if(!nodes) return -1;

    /* Temporary output stack for reversing LZW chains. */
    uint8_t *stack = (uint8_t *)malloc((size_t)maxsymbols);
    if(!stack)
    {
        free(nodes);
        return -1;
    }

    /* Initialize 256 single-byte entries. */
    for(int i = 0; i < 256; i++)
    {
        nodes[i].byte   = (uint8_t)i;
        nodes[i].parent = -1;
    }

    int reserved   = blockmode ? 1 : 0; /* code 256 reserved for clear */
    int numsymbols = 256 + reserved;
    int symbolsize = LZW_INITIAL_BITS;
    int prevsymbol = -1;
    int symcount   = 0; /* counter for block-mode alignment */

    BitStream bs;
    bitstream_init(&bs, src_buffer, src_size);

    size_t out_pos = 0;
    int    result  = 0;

    while(out_pos < dst_cap && !bitstream_eof(&bs))
    {
        int symbol = (int)bitstream_read_bits_le(&bs, symbolsize);
        symcount++;

        /* Block mode: code 256 clears the dictionary. */
        if(symbol == 256 && blockmode)
        {
            /* Skip remaining bits to next symbolsize-aligned boundary. */
            int rem = symcount % 8;
            if(rem)
            {
                for(int i = 0; i < 8 - rem; i++) bitstream_read_bits_le(&bs, symbolsize);
            }

            numsymbols = 256 + reserved;
            symbolsize = LZW_INITIAL_BITS;
            prevsymbol = -1;
            symcount   = 0;
            continue;
        }

        /* Resolve the symbol to output bytes. */
        int firstbyte;

        if(symbol < numsymbols)
        {
            /* Known symbol: find its first byte for new entry. */
            int s = symbol;
            while(nodes[s].parent >= 0) s = nodes[s].parent;
            firstbyte = nodes[s].byte;
        }
        else if(symbol == numsymbols && prevsymbol >= 0)
        {
            /* KwKwK case: first byte of previous symbol. */
            int s = prevsymbol;
            while(nodes[s].parent >= 0) s = nodes[s].parent;
            firstbyte = nodes[s].byte;
        }
        else
        {
            result = -1;
            break;
        }

        /* Add new dictionary entry. */
        if(prevsymbol >= 0 && numsymbols < maxsymbols)
        {
            nodes[numsymbols].parent = prevsymbol;
            nodes[numsymbols].byte   = (uint8_t)firstbyte;
            numsymbols++;

            /* Grow code width when numsymbols reaches next power of two. */
            if(numsymbols < maxsymbols && (numsymbols & (numsymbols - 1)) == 0) symbolsize++;
        }

        prevsymbol = symbol;

        /* Output the symbol's bytes (reverse chain, then emit). */
        int n = 0;
        int s = symbol;
        while(s >= 0)
        {
            if(n >= maxsymbols)
            {
                result = -1;
                goto done;
            }
            stack[n++] = nodes[s].byte;
            s          = nodes[s].parent;
        }

        for(int i = n - 1; i >= 0; i--)
        {
            if(out_pos >= dst_cap) break;
            dst_buffer[out_pos++] = stack[i];
        }
    }

done:
    free(stack);
    free(nodes);
    *dst_size = out_pos;
    return result;
}
