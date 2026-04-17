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

#include "bwt.h"

#include <stdlib.h>
#include <string.h>

/* Compute the inverse BWT permutation table */
void stuffit_calculate_inverse_bwt(uint32_t *transform, uint8_t *block, int blocklen)
{
    int counts[256];
    int cumulative[256];

    memset(counts, 0, sizeof(counts));
    for(int i = 0; i < blocklen; i++) counts[block[i]]++;

    int total = 0;
    for(int i = 0; i < 256; i++)
    {
        cumulative[i] = total;
        total += counts[i];
        counts[i] = 0;
    }

    for(int i = 0; i < blocklen; i++)
    {
        transform[cumulative[block[i]] + counts[block[i]]] = i;
        counts[block[i]]++;
    }
}

/* Unsort a BWT-transformed block */
void stuffit_unsort_bwt(uint8_t *dest, uint8_t *src, int blocklen, int firstindex, uint32_t *transform)
{
    stuffit_calculate_inverse_bwt(transform, src, blocklen);

    int idx = firstindex;
    for(int i = 0; i < blocklen; i++)
    {
        idx     = transform[idx];
        dest[i] = src[idx];
    }
}

/* Unsort an ST4 (Scrambled Transformation v4) block */
bool stuffit_unsort_st4(uint8_t *dest, uint8_t *src, int blocklen, int firstindex, uint32_t *transform)
{
    int  counts[256];
    int *pair_counts = calloc(sizeof(int), 256 * 256);
    if(!pair_counts) return false;

    for(int i = 0; i < 256; i++) counts[i] = 0;
    for(int i = 0; i < blocklen; i++) counts[src[i]]++;

    int total = 0;
    for(int i = 0; i < 256; i++)
    {
        int count = counts[i];
        counts[i] = total;
        for(int j = 0; j < count; j++) pair_counts[(src[total + j] << 8) | i]++;
        total += count;
    }

    uint8_t *bitvec = dest;
    memset(bitvec, 0, (blocklen + 7) / 8);

    int      array3[256];
    uint32_t counts2[256];

    for(int i = 0; i < 256; i++) array3[i] = -1;
    memcpy(counts2, counts, sizeof(counts));

    total = 0;
    for(int i = 0; i < 0x10000; i++)
    {
        int count = pair_counts[i];
        for(int j = 0; j < count; j++)
        {
            int byte = src[total + j];
            if(array3[byte] != total)
            {
                array3[byte] = total;
                int x        = counts[byte];
                bitvec[x >> 3] |= 1 << (x & 7);
            }
            counts[byte]++;
        }
        total += count;
    }

    for(int i = 0; i < 256; i++) array3[i] = 0;

    int index = 0;
    for(int i = 0; i < blocklen; i++)
    {
        if(bitvec[i / 8] & (1 << (i & 7))) index = i;

        int byte = src[i];
        if(index < array3[byte])
            transform[i] = (array3[byte] - 1) | 0x800000;
        else
        {
            transform[i] = counts2[byte];
            array3[byte] = i + 1;
        }
        counts2[byte]++;
        transform[i] |= byte << 24;
    }

    index         = firstindex;
    uint32_t tval = transform[firstindex];

    for(int i = 0; i < blocklen; i++)
    {
        if(tval & 0x800000)
        {
            int newindex = tval & 0x7fffff;
            if(newindex >= blocklen)
            {
                free(pair_counts);
                return false;
            }
            index = transform[newindex] & 0x7fffff;
            transform[newindex]++;
        }
        else
        {
            if(index >= blocklen)
            {
                free(pair_counts);
                return false;
            }
            transform[index]++;
            index = tval & 0x7fffff;
        }

        if(index >= blocklen)
        {
            free(pair_counts);
            return false;
        }
        tval    = transform[index];
        dest[i] = tval >> 24;
    }

    free(pair_counts);
    return true;
}

/* Reset the MTF decoder to identity permutation */
void stuffit_reset_mtf(StuffitMTFState *self)
{
    for(int i = 0; i < 256; i++) self->table[i] = i;
}

/* Decode one MTF symbol */
int stuffit_decode_mtf(StuffitMTFState *self, int symbol)
{
    int res = self->table[symbol];
    for(int i = symbol; i > 0; i--) self->table[i] = self->table[i - 1];
    self->table[0] = res;
    return res;
}

/* Decode an entire MTF-coded block in place */
void stuffit_decode_mtf_block(uint8_t *block, int blocklen)
{
    StuffitMTFState mtf;
    stuffit_reset_mtf(&mtf);
    for(int i = 0; i < blocklen; i++) block[i] = stuffit_decode_mtf(&mtf, block[i]);
}

/* Decode an M1FF-N block in place (Cyanide/Iron variant) */
void stuffit_decode_m1ff_block(uint8_t *block, int blocklen, int order)
{
    StuffitMTFState mtf;
    stuffit_reset_mtf(&mtf);
    int lasthead = order - 1;

    for(int i = 0; i < blocklen; i++)
    {
        int symbol = block[i];
        block[i]   = mtf.table[symbol];

        if(symbol == 0) { lasthead = 0; }
        else if(symbol == 1)
        {
            if(lasthead >= order)
            {
                int val      = mtf.table[1];
                mtf.table[1] = mtf.table[0];
                mtf.table[0] = val;
            }
        }
        else
        {
            int val = mtf.table[symbol];
            for(int j = symbol; j > 1; j--) mtf.table[j] = mtf.table[j - 1];
            mtf.table[1] = val;
        }

        lasthead++;
    }
}
