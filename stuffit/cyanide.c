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

/* StuffIt X method 1: Cyanide (BWT + Ternary Range-Coded MTF) */

#include <stdlib.h>
#include <string.h>
#include "bwt.h"
#include "rangecoder.h"
#include "stuffit.h"

typedef struct CyanideModel
{
    int      num;
    uint32_t frequencies[256];
    uint32_t mapping[256];
} CyanideModel;

static void cyanide_model_init(CyanideModel *m, int n)
{
    m->num = n;
    for(int i = 0; i < n; i++)
    {
        m->frequencies[i] = 1;
        m->mapping[i]     = n - 1 - i;
    }
}

static int cyanide_model_bump(CyanideModel *m, int index, int maxtotal)
{
    uint32_t total = 0;
    for(int i = 0; i < m->num; i++) total += m->frequencies[i];
    if(total >= (uint32_t)maxtotal)
        for(int i = 0; i < m->num; i++) m->frequencies[i] = (m->frequencies[i] + 1) / 2;

    uint32_t freq = m->frequencies[index];
    int      last = index;
    while(last < m->num - 1 && m->frequencies[last + 1] == freq) last++;
    if(last != index)
    {
        uint32_t tmp      = m->mapping[index];
        m->mapping[index] = m->mapping[last];
        m->mapping[last]  = tmp;
    }
    m->frequencies[last]++;
    return last;
}

static void calc_ternary_freqs(uint32_t *outf, uint32_t *meanings, uint32_t *inf)
{
    uint32_t a = inf[0], b = inf[1], c = inf[2];
    if(a < b)
    {
        if(a < c)
        {
            if(b < c)
            {
                meanings[0] = 0;
                meanings[1] = 1;
                meanings[2] = 2;
            }
            else
            {
                meanings[0] = 0;
                meanings[1] = 2;
                meanings[2] = 1;
            }
        }
        else
        {
            meanings[0] = 2;
            meanings[1] = 0;
            meanings[2] = 1;
        }
    }
    else
    {
        if(b < c)
        {
            if(c < a)
            {
                meanings[0] = 1;
                meanings[1] = 2;
                meanings[2] = 0;
            }
            else
            {
                meanings[0] = 1;
                meanings[1] = 0;
                meanings[2] = 2;
            }
        }
        else
        {
            meanings[0] = 2;
            meanings[1] = 1;
            meanings[2] = 0;
        }
    }
    outf[0] = inf[meanings[0]] + 1;
    outf[1] = inf[meanings[1]] + 1;
    outf[2] = inf[meanings[2]] + 1;
}

int stuffitx_cyanide_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size,
                                   size_t *src_consumed)
{
    static const int markovgroups[27] = {0, 1,  2,  3, 4, 5, 6, 7, 8, 3, 9,  10, 3, 4,
                                         5, 11, 11, 8, 6, 2, 5, 6, 7, 8, 12, 12, 13};

    size_t limit = *dst_size;
    size_t di    = 0;
    size_t si    = 1; /* skip initial byte */

    uint8_t  *block    = NULL;
    uint8_t  *sorted   = NULL;
    uint32_t *table    = NULL;
    int       currsize = 0;

    while(di < limit && si < src_size)
    {
        uint8_t marker = src[si++];
        if(marker == 0xff) break;
        if(marker != 0x77) return -1;

        if(si + 9 > src_size) return -1;
        uint32_t blocksize =
            ((uint32_t)src[si] << 24) | ((uint32_t)src[si + 1] << 16) | ((uint32_t)src[si + 2] << 8) | src[si + 3];
        si += 4;
        uint32_t firstindex =
            ((uint32_t)src[si] << 24) | ((uint32_t)src[si + 1] << 16) | ((uint32_t)src[si + 2] << 8) | src[si + 3];
        si += 4;
        int numsymbols = src[si++];

        if((int)blocksize > currsize)
        {
            free(block);
            block = malloc(blocksize * 6);
            if(!block) return -1;
            sorted   = block + blocksize;
            table    = (uint32_t *)(block + 2 * blocksize);
            currsize = blocksize;
        }

        /* Decode ternary-coded block */
        StuffitRangeCoder coder;
        stuffit_rc_init(&coder, src + si, src_size - si, NULL, true, 0x10000);

        uint32_t markovfreqs[14][3];
        memset(markovfreqs, 0, sizeof(markovfreqs));

        CyanideModel lowbitsmodels[8];
        int          b     = numsymbols;
        int          shift = 1;
        while(b > 0)
        {
            int n = 1 << shift;
            if(b < (3 << shift)) n = b;
            cyanide_model_init(&lowbitsmodels[shift - 1], n);
            b -= n;
            shift++;
        }

        CyanideModel highbitmodel;
        cyanide_model_init(&highbitmodel, shift);

        int prev = 0, prev2 = 0, prev3 = 0, someflag = 1;

        for(uint32_t i = 0; i < blocksize; i++)
        {
            int context = prev3 * 9 + prev2 * 3 + prev;
            int midx    = markovgroups[context];

            uint32_t freqs[3], meanings[3];
            calc_ternary_freqs(freqs, meanings, markovfreqs[midx]);
            int symbol = stuffit_rc_next_symbol(&coder, freqs, 3);
            int tresym = meanings[symbol];

            if(tresym == 0 && someflag == 0 && midx == 0)
            {
                someflag = 1;
                markovfreqs[midx][0] >>= 1;
                markovfreqs[midx][1] >>= 1;
                markovfreqs[midx][2] >>= 1;
                markovfreqs[midx][0] += 3;
                sorted[i] = 0;
            }
            else
            {
                if(tresym != 0) someflag = 0;
                uint32_t total  = freqs[0] + freqs[1] + freqs[2];
                uint32_t rlimit = someflag ? 4096 : 128;
                if(total > rlimit)
                {
                    markovfreqs[midx][0] >>= 1;
                    markovfreqs[midx][1] >>= 1;
                    markovfreqs[midx][2] >>= 1;
                }
                markovfreqs[midx][tresym] += 2;

                if(tresym <= 1)
                    sorted[i] = tresym;
                else
                {
                    int hbi      = stuffit_rc_next_symbol(&coder, highbitmodel.frequencies, highbitmodel.num);
                    int highbit  = highbitmodel.mapping[hbi];
                    int newindex = cyanide_model_bump(&highbitmodel, hbi, 0x100);
                    cyanide_model_bump(&highbitmodel, newindex, 0x10000);

                    if(highbit == 0)
                        sorted[i] = 2;
                    else
                    {
                        CyanideModel *lbm     = &lowbitsmodels[highbit - 1];
                        int           lbi     = stuffit_rc_next_symbol(&coder, lbm->frequencies, lbm->num);
                        int           lowbits = lbm->mapping[lbi];
                        int           max     = lbm->num * 128;
                        if(max > 0x4000) max = 0x4000;
                        cyanide_model_bump(lbm, lbi, max);
                        sorted[i] = (1 << highbit) + lowbits + 1;
                    }
                }
            }
            prev3 = prev2;
            prev2 = prev;
            prev  = tresym;
        }

        /* Update source position based on range coder consumption */
        si += coder.pos;

        stuffit_decode_m1ff_block(sorted, blocksize, 2);
        stuffit_unsort_bwt(block, sorted, blocksize, firstindex, table);

        /* Copy decompressed block to output */
        size_t copy = blocksize;
        if(di + copy > limit) copy = limit - di;
        memcpy(dst + di, block, copy);
        di += copy;
    }

    free(block);
    *dst_size = di;
    if(src_consumed) *src_consumed = si;
    return 0;
}
