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

/* StuffIt method 14: Block Huffman LZSS (256KB window) */

#include <stdlib.h>
#include <string.h>
#include "stuffit.h"
#include "stuffit_internal.h"

#define M14_WINDOW_SIZE 0x40000
#define M14_MAX_CODES   308

static void m14_update(uint16_t first, uint16_t last, uint8_t *code, uint16_t *freq)
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
                uint16_t t;
                uint8_t  tc;
                tc      = code[i];
                code[i] = code[j];
                code[j] = tc;
                t       = freq[i];
                freq[i] = freq[j];
                freq[j] = t;
            }
        } while(j > i);

        if(first != j)
        {
            uint16_t t;
            uint8_t  tc;
            tc          = code[first];
            code[first] = code[j];
            code[j]     = tc;
            t           = freq[first];
            freq[first] = freq[j];
            freq[j]     = t;
            i           = j + 1;
            if(last - i <= j - first)
            {
                m14_update(i, last, code, freq);
                last = j;
            }
            else
            {
                m14_update(first, j, code, freq);
                first = i;
            }
        }
        else
            ++first;
    }
}

static void m14_read_tree(StuffitBitReaderLE *br, uint16_t codesize, uint8_t *code, uint8_t *codecopy, uint16_t *freq,
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
        /* Recursive tree for meta-codes */
        uint16_t temp_result[64];
        uint8_t  temp_code[32], temp_copy[32];
        uint16_t temp_freq[64];
        uint32_t temp_buff[32];
        memset(temp_result, 0, sizeof(temp_result));
        m14_read_tree(br, size, temp_code, temp_copy, temp_freq, temp_buff, temp_result);

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

    for(uint32_t i = 0; i < codesize; i++)
    {
        codecopy[i] = code[i];
        freq[i]     = i;
    }
    m14_update(0, codesize, codecopy, freq);

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

    /* Align to byte boundary */
    stuffit_bits_le_init(br, br->data, br->data_len);
    br->byte_pos  = (br->byte_pos); /* already tracking position */
    /* The original aligns the IO to byte boundary after reading the tree.
       With our bit reader, remaining bits in the buffer are discarded. */
    br->bits_left = 0;
    br->buffer    = 0;
}

int stuffit_method14_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    uint8_t  *code     = calloc(M14_MAX_CODES, 1);
    uint8_t  *codecopy = calloc(M14_MAX_CODES, 1);
    uint16_t *freq     = calloc(M14_MAX_CODES, sizeof(uint16_t));
    uint32_t *buff     = calloc(M14_MAX_CODES, sizeof(uint32_t));
    uint16_t *var7     = calloc(M14_MAX_CODES * 2, sizeof(uint16_t));
    uint16_t *var3     = calloc(75 * 2, sizeof(uint16_t));

    uint8_t *window = calloc(M14_WINDOW_SIZE, 1);
    if(!code || !codecopy || !freq || !buff || !var7 || !var3 || !window) goto fail;

    /* Build lookup tables for lengths and distances */
    uint8_t  var1[52];
    uint16_t var2[52];
    uint8_t  var4[76];
    uint32_t var5[75];
    uint8_t  var8[0x4000];
    uint8_t  var6[1024];

    uint32_t i, k;
    for(i = k = 0; i < 52; i++)
    {
        var2[i] = k;
        var1[i] = (i >= 4) ? ((i - 4) >> 2) : 0;
        k += (1u << var1[i]);
    }
    for(i = 0; i < 4; i++) var8[i] = i;
    for(uint32_t m = 1, l = 4; i < 0x4000; m <<= 1)
        for(uint32_t n = l + 4; l < n; l++)
            for(uint32_t j = 0; j < m; j++) var8[i++] = l;

    for(i = 0, k = 1; i < 75; i++)
    {
        var5[i] = k;
        var4[i] = (i >= 3) ? ((i - 3) >> 2) : 0;
        k += (1u << var4[i]);
    }
    for(i = 0; i < 4; i++) var6[i] = i - 1;
    for(uint32_t m = 1, l = 3; i < 0x400; m <<= 1)
        for(uint32_t n = l + 4; l < n; l++)
            for(uint32_t j = 0; j < m; j++) var6[i++] = l;

    StuffitBitReaderLE br;
    stuffit_bits_le_init(&br, src, src_size);

    uint32_t num_blocks = stuffit_bits_le_read(&br, 16);
    uint32_t win_pos    = 0;

    for(uint32_t blk = 0; blk < num_blocks && di < limit; blk++)
    {
        stuffit_bits_le_read(&br, 16); /* skip crunched block size high */
        stuffit_bits_le_read(&br, 16); /* skip crunched block size low */
        uint32_t uncomp_size = stuffit_bits_le_read(&br, 16);
        uncomp_size |= (uint32_t)stuffit_bits_le_read(&br, 16) << 16;

        memset(var7, 0, M14_MAX_CODES * 2 * sizeof(uint16_t));
        memset(var3, 0, 75 * 2 * sizeof(uint16_t));
        m14_read_tree(&br, 308, code, codecopy, freq, buff, var7);
        m14_read_tree(&br, 75, code, codecopy, freq, buff, var3);

        while(uncomp_size > 0 && di < limit)
        {
            i = 0;
            while(i < 616) i = var7[i + stuffit_bits_le_read(&br, 1)];
            i -= 616;

            if(i < 0x100)
            {
                window[win_pos++] = (uint8_t)i;
                win_pos &= (M14_WINDOW_SIZE - 1);
                dst[di++] = (uint8_t)i;
                uncomp_size--;
            }
            else
            {
                i -= 0x100;
                k              = var2[i] + 4;
                uint32_t extra = var1[i];
                if(extra) k += stuffit_bits_le_read(&br, extra);

                i = 0;
                while(i < 150) i = var3[i + stuffit_bits_le_read(&br, 1)];
                i -= 150;

                uint32_t l = var5[i];
                extra      = var4[i];
                if(extra) l += stuffit_bits_le_read(&br, extra);

                uncomp_size -= k;
                l = win_pos + M14_WINDOW_SIZE - l;
                while(k-- && di < limit)
                {
                    l &= (M14_WINDOW_SIZE - 1);
                    uint8_t b         = window[l++];
                    window[win_pos++] = b;
                    win_pos &= (M14_WINDOW_SIZE - 1);
                    dst[di++] = b;
                }
            }
        }

        /* Byte-align after each block */
        br.bits_left = 0;
        br.buffer    = 0;
    }

    free(code);
    free(codecopy);
    free(freq);
    free(buff);
    free(var7);
    free(var3);
    free(window);
    *dst_size = di;
    return 0;

fail:
    free(code);
    free(codecopy);
    free(freq);
    free(buff);
    free(var7);
    free(var3);
    free(window);
    return -1;
}
