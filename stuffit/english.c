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

/* StuffIt X preprocessor 0: English dictionary word substitution */

#include <stdlib.h>
#include <string.h>
#include "../ppmd/SubAllocatorVariantI.h"
#include "../ppmd/VariantI.h"
#include "stuffit.h"

#define DICT_WORD_COUNT  100366
#define DICT_UNCOMP_SIZE 881863
#define DICT_COMP_SIZE   325602
#define DICT_CRC32       0xfb1dcfd5

extern unsigned char StuffItXEnglishDictionary[];

/* Static dictionary pointers - loaded once */
static const uint8_t **dict_ptrs = NULL;
static uint8_t        *dict_data = NULL;

static int dict_read_func(void *ctx)
{
    struct
    {
        const uint8_t *data;
        size_t         len;
        size_t         pos;
    } *r = ctx;

    if(r->pos >= r->len) return -1;
    return r->data[r->pos++];
}

static int load_dictionary(void)
{
    if(dict_ptrs) return 0;

    /* Decompress dictionary using PPMd Variant I */
    struct
    {
        const uint8_t *data;
        size_t         len;
        size_t         pos;
    } reader;

    reader.data = StuffItXEnglishDictionary;
    reader.len  = DICT_COMP_SIZE;
    reader.pos  = 0;

    PPMdSubAllocatorVariantI *alloc = CreateSubAllocatorVariantI(16 * 1024 * 1024);
    if(!alloc) return -1;

    PPMdModelVariantI model;
    StartPPMdModelVariantI(&model, dict_read_func, &reader, alloc, 16, 0);

    dict_data = malloc(DICT_UNCOMP_SIZE);
    if(!dict_data)
    {
        FreeSubAllocatorVariantI(alloc);
        return -1;
    }

    for(size_t i = 0; i < DICT_UNCOMP_SIZE; i++)
    {
        int b = NextPPMdVariantIByte(&model);
        if(b < 0)
        {
            free(dict_data);
            dict_data = NULL;
            FreeSubAllocatorVariantI(alloc);
            return -1;
        }
        dict_data[i] = (uint8_t)b;
    }

    FreeSubAllocatorVariantI(alloc);

    /* Build word pointer table */
    dict_ptrs = malloc(sizeof(uint8_t *) * (DICT_WORD_COUNT + 1));
    if(!dict_ptrs)
    {
        free(dict_data);
        dict_data = NULL;
        return -1;
    }

    dict_ptrs[0]     = dict_data;
    const uint8_t *p = dict_data;
    for(int i = 1; i <= DICT_WORD_COUNT; i++)
    {
        while(*p != 0x0a && p < dict_data + DICT_UNCOMP_SIZE) p++;
        dict_ptrs[i] = ++p;
    }

    return 0;
}

int stuffitx_english_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    if(load_dictionary() < 0) return -1;

    size_t limit = *dst_size;
    size_t si    = 0;
    size_t di    = 0;

    if(si + 4 > src_size) return -1;
    int esccode   = src[si++];
    int wordcode  = src[si++];
    int firstcode = src[si++];
    int uppercode = src[si++];

    int caseflag = 1;

    while(si < src_size && di < limit)
    {
        int c = src[si++];

        if(c == esccode)
        {
            caseflag = 0;
            if(si >= src_size) break;
            if(di < limit) dst[di++] = src[si++];
        }
        else if(c == wordcode || c == firstcode || c == uppercode)
        {
            int index = 0;
            int c2    = -1;

            for(;;)
            {
                if(si >= src_size)
                {
                    c2 = -1;
                    break;
                }
                c2 = src[si++];
                if((c2 < 'A' || c2 > 'Z') && (c2 < 'a' || c2 > 'z')) break;
                index *= 52;
                if(c2 <= 'Z')
                    index += c2 - 'A' + 26 + 1;
                else
                    index += c2 - 'a' + 1;
            }

            if(index >= DICT_WORD_COUNT) return -1;

            size_t  wlen = dict_ptrs[index + 1] - dict_ptrs[index] - 1;
            uint8_t wordbuf[256];
            if(wlen > 255) wlen = 255;
            memcpy(wordbuf, dict_ptrs[index], wlen);

            if(c == uppercode)
                for(size_t i = 0; i < wlen; i++) wordbuf[i] -= 32;
            else if(c == firstcode)
                wordbuf[0] -= 32;

            if(caseflag)
            {
                if(wordbuf[0] >= 'A' && wordbuf[0] <= 'Z')
                    wordbuf[0] += 32;
                else if(wordbuf[0] >= 'a' && wordbuf[0] <= 'z')
                    wordbuf[0] -= 32;
            }

            if(c2 == esccode && si < src_size) c2 = src[si++];
            if(c2 != -1) wordbuf[wlen++] = (uint8_t)c2;

            if(c2 == '.' || c2 == '?' || c2 == '!')
                caseflag = 1;
            else
                caseflag = 0;

            for(size_t i = 0; i < wlen && di < limit; i++) dst[di++] = wordbuf[i];
        }
        else
        {
            if(caseflag)
            {
                if(c >= 'A' && c <= 'Z')
                {
                    c += 32;
                    caseflag = 0;
                }
                else if(c >= 'a' && c <= 'z')
                {
                    c -= 32;
                    caseflag = 0;
                }
            }

            if(c == '.' || c == '?' || c == '!')
                caseflag = 1;
            else if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
                caseflag = 0;

            if(di < limit) dst[di++] = (uint8_t)c;
        }
    }

    *dst_size = di;
    return 0;
}
