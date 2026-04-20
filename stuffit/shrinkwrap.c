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
 * ShrinkWrap 3 "SIT2" decompressor
 *
 * Reverse-engineered from 68K 'Dcmp' resource ID 240 in ShrinkWrap 3.5.1.
 * Used by NDIF disk images (chunk type 0xF0) created by ShrinkWrap 3.
 * Component type 'Chdc', subtype 'SIT2', manufacturer 'Wrap'.
 *
 * Algorithm: Block Huffman LZSS, adapted from StuffIt method 14, with:
 * - 276 literal/length symbols, 16 distance symbols
 * - 20 length codes, 16 distance codes
 * - 16-byte per-chunk header
 */

#include <stdlib.h>
#include <string.h>
#include "stuffit.h"
#include "stuffit_internal.h"

#define SW_LITLEN 276
#define SW_DIST   16

/* Quicksort for canonical code assignment (same as method 14) */
static void sw_update(uint16_t first, uint16_t last, uint8_t *code, uint16_t *freq)
{
    while(last - first > 1)
    {
        uint16_t i = first, j = last;
        do
        {
            while(++i < last && code[first] > code[i]);
            while(--j > first && code[first] < code[j]);
            if(j > i)
            {
                uint8_t tc  = code[i];
                code[i]     = code[j];
                code[j]     = tc;
                uint16_t tf = freq[i];
                freq[i]     = freq[j];
                freq[j]     = tf;
            }
        } while(j > i);

        if(first != j)
        {
            uint8_t tc  = code[first];
            code[first] = code[j];
            code[j]     = tc;
            uint16_t tf = freq[first];
            freq[first] = freq[j];
            freq[j]     = tf;
            i           = j + 1;
            if(last - i <= j - first)
            {
                sw_update(i, last, code, freq);
                last = j;
            }
            else
            {
                sw_update(first, j, code, freq);
                first = i;
            }
        }
        else
            ++first;
    }
}

/* Read Huffman tree from bitstream (same structure as method 14) */
static void sw_read_tree(StuffitBitReaderLE *br, uint16_t codesize, uint8_t *code, uint8_t *codecopy, uint16_t *freq,
                         uint32_t *buff, uint16_t *result)
{
    uint32_t k    = stuffit_bits_le_read(br, 1);
    uint32_t j    = stuffit_bits_le_read(br, 2) + 2;
    uint32_t o    = stuffit_bits_le_read(br, 3) + 1;
    uint32_t size = 1u << j;
    uint32_t m    = size - 1;
    k             = k ? m - 1 : (uint32_t)-1;

    if(stuffit_bits_le_read(br, 2) & 1)
    {
        uint16_t temp_result[64];
        uint8_t  temp_code[32], temp_copy[32];
        uint16_t temp_freq[64];
        uint32_t temp_buff[32];
        memset(temp_result, 0, sizeof(temp_result));
        sw_read_tree(br, size, temp_code, temp_copy, temp_freq, temp_buff, temp_result);

        for(uint32_t i = 0; i < codesize;)
        {
            uint32_t l = 0, n;
            do
            {
                l = temp_result[l + stuffit_bits_le_read(br, 1)];
                n = size << 1;
            } while(n > l);
            l -= n;
            if(k != l)
            {
                if(l == m)
                {
                    l = 0;
                    do
                    {
                        l = temp_result[l + stuffit_bits_le_read(br, 1)];
                        n = size << 1;
                    } while(n > l);
                    l += 3 - n;
                    while(l-- && i < codesize)
                    {
                        code[i] = code[i - 1];
                        i++;
                    }
                }
                else
                    code[i++] = l + o;
            }
            else
                code[i++] = 0;
        }
    }
    else
    {
        for(uint32_t i = 0; i < codesize;)
        {
            uint32_t l = stuffit_bits_le_read(br, j);
            if(k != l)
            {
                if(l == m)
                {
                    l = stuffit_bits_le_read(br, j) + 3;
                    while(l-- && i < codesize)
                    {
                        code[i] = code[i - 1];
                        i++;
                    }
                }
                else
                    code[i++] = l + o;
            }
            else
                code[i++] = 0;
        }
    }

    /* Sort + canonical code assignment + tree build */
    for(uint32_t i = 0; i < codesize; i++)
    {
        codecopy[i] = code[i];
        freq[i]     = i;
    }
    sw_update(0, codesize, codecopy, freq);

    uint32_t i;
    for(i = 0; i < codesize && !codecopy[i]; i++);
    for(j = 0; i < codesize; i++, j++)
    {
        if(i) j <<= (codecopy[i] - codecopy[i - 1]);
        k = codecopy[i];
        m = 0;
        for(uint32_t l = j; k--; l >>= 1) m = (m << 1) | (l & 1);
        buff[freq[i]] = m;
    }

    for(i = 0; i < codesize * 2; i++) result[i] = 0;
    j = 2;
    for(i = 0; i < codesize; i++)
    {
        uint32_t l = 0;
        m          = buff[i];
        for(k = 0; k < code[i]; k++)
        {
            l += (m & 1);
            if(code[i] - 1 <= k)
                result[l] = codesize * 2 + i;
            else
            {
                if(!result[l])
                {
                    result[l] = j;
                    j += 2;
                }
                l = result[l];
            }
            m >>= 1;
        }
    }

    /* Byte-align after tree read (68K code length reader returns byte-aligned) */
    br->bits_left = 0;
    br->buffer    = 0;
}

int stuffit_shrinkwrap_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    if(src_size < 16) return -1;

    /* 16-byte header: skip4 | LE32 decomp_size | LE32 decomp_size | FFFFFFFF */
    uint32_t decomp_size =
        (uint32_t)src[4] | ((uint32_t)src[5] << 8) | ((uint32_t)src[6] << 16) | ((uint32_t)src[7] << 24);
    if(decomp_size > *dst_size) return -1;

    /* Build length/distance tables */
    uint8_t  len_extra[20];
    uint16_t len_base[20];
    uint8_t  dist_extra[16];
    uint16_t dist_base[16];

    uint32_t i, k;
    for(i = 0, k = 0; i < 20; i++)
    {
        len_base[i]  = k + 4;
        len_extra[i] = (i >= 2) ? ((i - 2) >> 1) : 0;
        k += (1u << len_extra[i]);
    }
    for(i = 0, k = 1; i < 16; i++)
    {
        dist_base[i]  = k;
        dist_extra[i] = i;
        k += (1u << i);
    }

    /* Allocate workspace */
    uint8_t  *code        = calloc(SW_LITLEN + 64, 1);
    uint8_t  *codecopy    = calloc(SW_LITLEN + 64, 1);
    uint16_t *freq        = calloc(SW_LITLEN + 64, sizeof(uint16_t));
    uint32_t *buff        = calloc(SW_LITLEN + 64, sizeof(uint32_t));
    uint16_t *litlen_tree = calloc(SW_LITLEN * 8, sizeof(uint16_t));
    uint16_t *dist_tree   = calloc(4096, sizeof(uint16_t));

    if(!code || !codecopy || !freq || !buff || !litlen_tree || !dist_tree) goto fail;

    StuffitBitReaderLE br;
    stuffit_bits_le_init(&br, src + 16, src_size - 16);

    /* Read litlen tree (276 symbols) */
    memset(litlen_tree, 0, SW_LITLEN * 8 * sizeof(uint16_t));
    sw_read_tree(&br, SW_LITLEN, code, codecopy, freq, buff, litlen_tree);

    /* Read dist tree (16 symbols) */
    memset(code, 0, SW_LITLEN + 64);
    memset(dist_tree, 0, 4096 * sizeof(uint16_t));
    sw_read_tree(&br, SW_DIST, code, codecopy, freq, buff, dist_tree);

    /* Main decode loop */
    size_t   di        = 0;
    uint32_t litlen_th = SW_LITLEN * 2;
    uint32_t dist_th   = SW_DIST * 2;

    while(di < decomp_size)
    {
        /* Decode literal/length symbol */
        i = 0;
        while(i < litlen_th) i = litlen_tree[i + stuffit_bits_le_read(&br, 1)];
        i -= litlen_th;

        if(i < 256) { dst[di++] = (uint8_t)i; }
        else
        {
            i -= 256;
            if(i >= 20) goto fail;

            k              = len_base[i];
            uint32_t extra = len_extra[i];
            if(extra) k += stuffit_bits_le_read(&br, extra);

            /* Decode distance */
            uint32_t d = 0;
            while(d < dist_th) d = dist_tree[d + stuffit_bits_le_read(&br, 1)];
            d -= dist_th;
            if(d >= 16) goto fail;

            uint32_t dist = dist_base[d];
            extra         = dist_extra[d];
            if(extra) dist += stuffit_bits_le_read(&br, extra);

            if(dist > di) goto fail;

            /* Copy from back-reference */
            for(uint32_t j2 = 0; j2 < k && di < decomp_size; j2++)
            {
                dst[di] = dst[di - dist];
                di++;
            }
        }
    }

    free(code);
    free(codecopy);
    free(freq);
    free(buff);
    free(litlen_tree);
    free(dist_tree);
    *dst_size = di;
    return 0;

fail:
    free(code);
    free(codecopy);
    free(freq);
    free(buff);
    free(litlen_tree);
    free(dist_tree);
    return -1;
}
