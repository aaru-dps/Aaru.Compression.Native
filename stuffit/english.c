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

#if defined(_WIN32)
#include <windows.h>
#define AARU_ENGLISH_ONCE_WIN32 1
#elif defined(__unix__) || defined(__APPLE__) || defined(_POSIX_VERSION)
#include <pthread.h>
#define AARU_ENGLISH_ONCE_PTHREAD 1
#endif

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

/* Result of the one-time dictionary build; 0 on success, -1 on failure. A failed build is
   recorded and never retried. */
static int dict_status = -1;

/* Builds the shared dictionary. Runs exactly once; publishes the statics only once both the
   word data and the fully populated pointer table are complete. */
static void build_dictionary(void)
{
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
    if(!alloc) return;

    PPMdModelVariantI model;
    StartPPMdModelVariantI(&model, dict_read_func, &reader, alloc, 16, 0);

    uint8_t *data = malloc(DICT_UNCOMP_SIZE);
    if(!data)
    {
        FreeSubAllocatorVariantI(alloc);
        return;
    }

    for(size_t i = 0; i < DICT_UNCOMP_SIZE; i++)
    {
        int b = NextPPMdVariantIByte(&model);
        if(b < 0)
        {
            free(data);
            FreeSubAllocatorVariantI(alloc);
            return;
        }
        data[i] = (uint8_t)b;
    }

    FreeSubAllocatorVariantI(alloc);

    /* Build word pointer table */
    const uint8_t **ptrs = malloc(sizeof(uint8_t *) * (DICT_WORD_COUNT + 1));
    if(!ptrs)
    {
        free(data);
        return;
    }

    ptrs[0]          = data;
    const uint8_t *p = data;
    for(int i = 1; i <= DICT_WORD_COUNT; i++)
    {
        while(p < data + DICT_UNCOMP_SIZE && *p != 0x0a) p++;
        ptrs[i] = ++p;
    }

    /* Publish only now that the table is fully populated. */
    dict_data   = data;
    dict_ptrs   = ptrs;
    dict_status = 0;
}

#if defined(AARU_ENGLISH_ONCE_WIN32)
static INIT_ONCE dict_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK build_dictionary_once(PINIT_ONCE once, PVOID param, PVOID *context)
{
    (void)once;
    (void)param;
    (void)context;
    build_dictionary();
    return TRUE;
}
#elif defined(AARU_ENGLISH_ONCE_PTHREAD)
static pthread_once_t dict_once = PTHREAD_ONCE_INIT;
#else
static int dict_once_done = 0;
#endif

static int load_dictionary(void)
{
#if defined(AARU_ENGLISH_ONCE_WIN32)
    InitOnceExecuteOnce(&dict_once, build_dictionary_once, NULL, NULL);
#elif defined(AARU_ENGLISH_ONCE_PTHREAD)
    pthread_once(&dict_once, build_dictionary);
#else
    /* No one-shot primitive available; single-threaded use only. */
    if(!dict_once_done)
    {
        dict_once_done = 1;
        build_dictionary();
    }
#endif

    return dict_status;
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

                /* Every further digit only grows the index, so bail out at once rather than
                   letting it overflow into a value that passes the bounds check below. */
                if(index >= DICT_WORD_COUNT) return -1;
            }

            if(index >= DICT_WORD_COUNT) return -1;

            size_t  wlen = dict_ptrs[index + 1] - dict_ptrs[index] - 1;
            uint8_t wordbuf[256];
            /* Leave room for the trailing terminator character appended below. */
            if(wlen > sizeof(wordbuf) - 2) wlen = sizeof(wordbuf) - 2;
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
