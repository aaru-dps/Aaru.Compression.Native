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

/* StuffIt X method 6: Iron (Advanced BWT/ST4 + Fancy MTF + Range Coder) */

#include <stdlib.h>
#include <string.h>
#include "bwt.h"
#include "rangecoder.h"
#include "stuffit.h"
#include "stuffit_internal.h"

static int iron_bit_w(StuffitRangeCoder *c, uint32_t *w, int shift)
{
    int bit = stuffit_rc_next_weighted_bit(c, *w, 0x1000);
    if(bit == 0)
        *w += (0x1000 - *w) >> shift;
    else
        *w -= *w >> shift;
    return bit;
}

static int iron_bit_dw(StuffitRangeCoder *c, uint32_t *w1, int s1, uint32_t *w2, int s2)
{
    int bit = stuffit_rc_next_weighted_bit(c, (*w1 + *w2) / 2, 0x1000);
    if(bit == 0)
    {
        *w1 += (0x1000 - *w1) >> s1;
        *w2 += (0x1000 - *w2) >> s2;
    }
    else
    {
        *w1 -= *w1 >> s1;
        *w2 -= *w2 >> s2;
    }
    return bit;
}

int stuffitx_iron_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    StuffitBitReaderLE br;
    stuffit_bits_le_init(&br, src, src_size);

    int st4      = stuffit_bits_le_read_bit(&br);
    int fancymtf = stuffit_bits_le_read_bit(&br);

    uint32_t     maxfreq1    = 1u << (uint32_t)stuffit_read_p2(&br);
    uint32_t     maxfreq2    = 1u << (uint32_t)stuffit_read_p2(&br);
    uint32_t     maxfreq3    = 1u << (uint32_t)stuffit_read_p2(&br);
    unsigned int byteshift1  = (unsigned int)stuffit_read_p2(&br);
    unsigned int byteshift2  = (unsigned int)stuffit_read_p2(&br);
    unsigned int byteshift3  = (unsigned int)stuffit_read_p2(&br);
    unsigned int countshift1 = (unsigned int)stuffit_read_p2(&br);
    unsigned int countshift2 = (unsigned int)stuffit_read_p2(&br);
    unsigned int countshift3 = (unsigned int)stuffit_read_p2(&br);

    uint8_t  *block    = NULL;
    uint8_t  *sorted   = NULL;
    uint32_t *table    = NULL;
    int       currsize = 0;

    /* Record current byte position for range coder init */
    size_t block_data_pos = br.byte_pos;

    while(di < limit)
    {
        /* Align to byte boundary */
        StuffitBitReaderLE br2;
        stuffit_bits_le_init(&br2, src + block_data_pos, src_size - block_data_pos);

        if(stuffit_bits_le_read_bit(&br2) == 1) break; /* end marker */

        unsigned int blocksize = (unsigned int)stuffit_read_p2(&br2);
        if(blocksize == 0) break;

        if((int)blocksize > currsize)
        {
            free(block);
            block = malloc(blocksize * 6);
            if(!block) return -1;
            sorted   = block + blocksize;
            table    = (uint32_t *)(block + 2 * blocksize);
            currsize = blocksize;
        }

        if(stuffit_bits_le_read_bit(&br2) == 0)
        {
            /* Compressed block */
            unsigned int firstindex = (unsigned int)stuffit_read_p2(&br2);
            if(firstindex >= blocksize)
            {
                free(block);
                return -1;
            }

            /* Align + skip 1 byte + init range coder */
            size_t coder_start = block_data_pos + br2.byte_pos;
            coder_start++; /* skip 1 byte */

            StuffitRangeCoder coder;
            stuffit_rc_init(&coder, src + coder_start, src_size - coder_start, NULL, false, 0);

            /* Decode block content */
            uint32_t mainfreqs[4] = {1, 1, 1, 1};
            uint32_t lastbytefreqs[256][4];
            memset(lastbytefreqs, 0, sizeof(lastbytefreqs));
            uint32_t somethingfreqs[4][256][4];
            memset(somethingfreqs, 0, sizeof(somethingfreqs));
            uint32_t bytelen_w[8], bytelen_w2[8][8], bytebit_w[8][128];
            uint32_t countlen_w[4][16][24], countlen_w2[256][24], countbit_w[24][24];

            for(int i = 0; i < 8; i++) bytelen_w[i] = 0x800;
            for(int i = 0; i < 8; i++)
                for(int j = 0; j < 8; j++) bytelen_w2[i][j] = 0x800;
            for(int i = 0; i < 8; i++)
                for(int j = 0; j < 128; j++) bytebit_w[i][j] = 0x800;
            for(int i = 0; i < 4; i++)
                for(int j = 0; j < 16; j++)
                    for(int k = 0; k < 24; k++) countlen_w[i][j][k] = 0x800;
            for(int i = 0; i < 256; i++)
                for(int j = 0; j < 24; j++) countlen_w2[i][j] = 0x800;
            for(int i = 0; i < 24; i++)
                for(int j = 0; j < 24; j++) countbit_w[i][j] = 0x800;

            uint8_t  mtfbuf[256];
            uint32_t numbytes = 0;
            uint32_t ia1[257], ia2[257], ia3[257];

            if(fancymtf)
            {
                for(int i = 0; i < 257; i++)
                {
                    ia1[i] = i;
                    ia2[i] = i;
                    ia3[i] = 0;
                }
                ia3[256] = (uint32_t)-1;
            }
            else
                for(int i = 0; i < 256; i++) mtfbuf[i] = i;

            int valhist = 0, lenhist = 0, lastbits = 0, lastbyte = 0;

            for(unsigned int bi = 0; bi < blocksize;)
            {
                uint32_t *f1 = mainfreqs;
                uint32_t *f2 = lastbytefreqs[lastbyte];
                uint32_t *f3 = somethingfreqs[lenhist & 3][valhist];
                uint32_t  freqs[4];
                for(int j = 0; j < 4; j++) freqs[j] = f1[j] + f2[j] + f3[j];

                int symbol = stuffit_rc_next_symbol(&coder, freqs, 4);
                f1[symbol] += 2;
                f2[symbol] += 2;
                f3[symbol] += 2;

                uint32_t t1 = 0, t2 = 0, t3 = 0;
                for(int j = 0; j < 4; j++)
                {
                    t1 += f1[j];
                    t2 += f2[j];
                    t3 += f3[j];
                }
                if(t1 > maxfreq1)
                    for(int j = 0; j < 4; j++) f1[j] = (f1[j] + 1) / 2;
                if(t2 > maxfreq2)
                    for(int j = 0; j < 4; j++) f2[j] /= 2;
                if(t3 > maxfreq3)
                    for(int j = 0; j < 4; j++) f3[j] /= 2;

                int value;
                if(symbol != 3)
                    value = symbol;
                else
                {
                    int bits = 0;
                    while(bits < 6)
                    {
                        if(iron_bit_dw(&coder, &bytelen_w[bits], byteshift1, &bytelen_w2[lastbits][bits], byteshift2) ==
                           0)
                            break;
                        bits++;
                    }
                    value = 1;
                    for(int j = 0; j <= bits; j++)
                        value = (value << 1) | iron_bit_w(&coder, &bytebit_w[bits][value], byteshift3);
                    value++;
                    lastbits = bits;
                }

                int byte;
                if(fancymtf)
                {
                    int index       = (value + 1) & 0xff;
                    byte            = ia1[index] & 0xff;
                    block[numbytes] = byte;
                    ia3[byte] += 0x4000;

                    for(int ii = (int)ia2[byte]; ii > 0; ii--)
                    {
                        ia1[ii]          = ia1[ii - 1];
                        ia2[ia1[ii - 1]] = ii;
                    }
                    ia1[0]    = byte;
                    ia2[byte] = 0;

                    for(int j = 0; j < 12; j++)
                    {
                        int n = 1 << j;
                        if((uint32_t)n <= numbytes)
                        {
                            int b2 = block[numbytes - n];
                            if(j == 0)
                                ia3[b2] -= 0x3801;
                            else
                                ia3[b2] -= 0x800 >> j;
                            if(b2 != byte)
                            {
                                uint32_t v = ia2[b2];
                                while(ia3[ia1[v + 1]] > ia3[b2])
                                {
                                    ia1[v]          = ia1[v + 1];
                                    ia2[ia1[v + 1]] = v;
                                    v++;
                                }
                                ia1[v]  = b2;
                                ia2[b2] = v;
                            }
                        }
                    }
                    numbytes++;
                }
                else
                {
                    int index = (value + 1) & 0xff;
                    byte      = mtfbuf[index];
                    memmove(mtfbuf + 1, mtfbuf, index);
                    mtfbuf[0] = byte;
                }

                int sv = value <= 3 ? value : 3;

                int bits = 0;
                for(;;)
                {
                    if(iron_bit_dw(&coder, &countlen_w[sv][lenhist][bits], countshift1, &countlen_w2[byte][bits],
                                   countshift2) == 0)
                        break;
                    bits++;
                    if(bits >= 24)
                    {
                        free(block);
                        return -1;
                    }
                }

                int cnt = 1;
                for(int j = 0; j < bits; j++) cnt = (cnt << 1) | iron_bit_w(&coder, &countbit_w[bits][j], countshift3);

                for(int j = 0; j < cnt; j++)
                {
                    if(bi >= blocksize)
                    {
                        free(block);
                        return -1;
                    }
                    sorted[bi++] = byte;
                }

                valhist = ((valhist << 2) | sv) & 0xff;
                lenhist = ((lenhist << 1) & 0x0e);
                if(bits > 1) lenhist |= 1;
                lastbyte = byte;
            }

            /* Apply inverse transform */
            if(st4)
            {
                if(!stuffit_unsort_st4(block, sorted, blocksize, firstindex, table))
                {
                    free(block);
                    return -1;
                }
            }
            else
                stuffit_unsort_bwt(block, sorted, blocksize, firstindex, table);

            block_data_pos = coder_start + coder.pos;
        }
        else
        {
            /* Uncompressed block */
            size_t raw_start = block_data_pos + br2.byte_pos;
            if(raw_start + blocksize > src_size)
            {
                free(block);
                return -1;
            }
            memcpy(block, src + raw_start, blocksize);
            block_data_pos = raw_start + blocksize;
        }

        /* Output block */
        size_t copy = blocksize;
        if(di + copy > limit) copy = limit - di;
        memcpy(dst + di, block, copy);
        di += copy;
    }

    free(block);
    *dst_size = di;
    return 0;
}
