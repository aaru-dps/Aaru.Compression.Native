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

/* StuffIt method 15: Arsenic (BWT + adaptive arithmetic coding + RLE) */

#include <stdlib.h>
#include <string.h>
#include "bwt.h"
#include "stuffit.h"

#define ARSENIC_NUM_BITS 26
#define ARSENIC_ONE      (1 << (ARSENIC_NUM_BITS - 1))
#define ARSENIC_HALF     (1 << (ARSENIC_NUM_BITS - 2))

typedef struct ArithSymbol
{
    int symbol;
    int frequency;
} ArithSymbol;

typedef struct ArithModel
{
    int         totalfreq, increment, freqlimit, numsymbols;
    ArithSymbol symbols[256];
} ArithModel;

typedef struct ArithDecoder
{
    const uint8_t *data;
    size_t         len;
    size_t         pos;
    uint32_t       range, code;
} ArithDecoder;

static void arith_model_init(ArithModel *m, int first, int last, int inc, int flimit)
{
    m->increment  = inc;
    m->freqlimit  = flimit;
    m->numsymbols = last - first + 1;
    m->totalfreq  = inc * m->numsymbols;
    for(int i = 0; i < m->numsymbols; i++)
    {
        m->symbols[i].symbol    = i + first;
        m->symbols[i].frequency = inc;
    }
}

static void arith_model_reset(ArithModel *m)
{
    m->totalfreq = m->increment * m->numsymbols;
    for(int i = 0; i < m->numsymbols; i++) m->symbols[i].frequency = m->increment;
}

static void arith_model_bump(ArithModel *m, int idx)
{
    m->symbols[idx].frequency += m->increment;
    m->totalfreq += m->increment;
    if(m->totalfreq > m->freqlimit)
    {
        m->totalfreq = 0;
        for(int i = 0; i < m->numsymbols; i++)
        {
            m->symbols[i].frequency++;
            m->symbols[i].frequency >>= 1;
            m->totalfreq += m->symbols[i].frequency;
        }
    }
}

static uint8_t arith_read_byte(ArithDecoder *d) { return (d->pos < d->len) ? d->data[d->pos++] : 0; }

static void arith_dec_init(ArithDecoder *d, const uint8_t *data, size_t len)
{
    d->data  = data;
    d->len   = len;
    d->pos   = 0;
    d->range = ARSENIC_ONE;
    d->code  = 0;
    for(int i = 0; i < ARSENIC_NUM_BITS; i++) d->code = (d->code << 1) | ((arith_read_byte(d) >> (7 - (i & 7))) & 1);
    /* Actually read ARSENIC_NUM_BITS bits MSB-first */
    d->pos       = 0;
    d->code      = 0;
    /* Re-read properly: 26 bits from the stream */
    uint32_t val = 0;
    for(int i = 0; i < 4; i++) val = (val << 8) | d->data[d->pos++];
    d->code = val >> (32 - ARSENIC_NUM_BITS);
}

static void arith_normalize(ArithDecoder *d)
{
    while(d->range <= ARSENIC_HALF)
    {
        d->range <<= 1;
        d->code = (d->code << 1) | ((d->pos < d->len && (d->data[d->pos / 1] >> (7 - 0)) & 1) ? 1 : 0);
        /* Read one bit MSB-first */
        /* Actually we need a proper bit reader here. Let me use a bit buffer. */
    }
}

/* Better approach: use a bitstream-based arithmetic decoder */

typedef struct ArsenicDecoder
{
    const uint8_t *data;
    size_t         len;
    size_t         byte_pos;
    int            bits_left;
    uint32_t       bit_buf;
    uint32_t       range;
    uint32_t       code;
} ArsenicDecoder;

static int arsenic_read_bit(ArsenicDecoder *d)
{
    if(d->bits_left == 0)
    {
        d->bit_buf   = (d->byte_pos < d->len) ? d->data[d->byte_pos++] : 0;
        d->bits_left = 8;
    }
    d->bits_left--;
    return (d->bit_buf >> d->bits_left) & 1;
}

static uint32_t arsenic_read_bits(ArsenicDecoder *d, int n)
{
    uint32_t val = 0;
    for(int i = 0; i < n; i++) val = (val << 1) | arsenic_read_bit(d);
    return val;
}

static void arsenic_init(ArsenicDecoder *d, const uint8_t *data, size_t len)
{
    d->data      = data;
    d->len       = len;
    d->byte_pos  = 0;
    d->bits_left = 0;
    d->bit_buf   = 0;
    d->range     = ARSENIC_ONE;
    d->code      = arsenic_read_bits(d, ARSENIC_NUM_BITS);
}

static void arsenic_normalize(ArsenicDecoder *d)
{
    while(d->range <= ARSENIC_HALF)
    {
        d->range <<= 1;
        d->code = (d->code << 1) | arsenic_read_bit(d);
    }
}

static void arsenic_read_code(ArsenicDecoder *d, int symlow, int symsize, int symtot)
{
    int renorm  = d->range / symtot;
    int lowincr = renorm * symlow;
    d->code -= lowincr;
    if(symlow + symsize == symtot)
        d->range -= lowincr;
    else
        d->range = symsize * renorm;
    arsenic_normalize(d);
}

static int arsenic_next_symbol(ArsenicDecoder *d, ArithModel *m)
{
    int freq = d->code / (d->range / m->totalfreq);
    int cum  = 0, n;
    for(n = 0; n < m->numsymbols - 1; n++)
    {
        if(cum + m->symbols[n].frequency > freq) break;
        cum += m->symbols[n].frequency;
    }
    arsenic_read_code(d, cum, m->symbols[n].frequency, m->totalfreq);
    arith_model_bump(m, n);
    return m->symbols[n].symbol;
}

static int arsenic_next_bitstring(ArsenicDecoder *d, ArithModel *m, int bits)
{
    int res = 0;
    for(int i = 0; i < bits; i++)
        if(arsenic_next_symbol(d, m)) res |= 1 << i;
    return res;
}

static const uint16_t arsenic_rand_table[256] = {
    0xee, 0x56, 0xf8,  0xc3, 0x9d, 0x9f,  0xae, 0x2c, 0xad, 0xcd,  0x24, 0x9d, 0xa6, 0x101, 0x18, 0xb9, 0xa1, 0x82,
    0x75, 0xe9, 0x9f,  0x55, 0x66, 0x6a,  0x86, 0x71, 0xdc, 0x84,  0x56, 0x96, 0x56, 0xa1,  0x84, 0x78, 0xb7, 0x32,
    0x6a, 0x03, 0xe3,  0x02, 0x11, 0x101, 0x08, 0x44, 0x83, 0x100, 0x43, 0xe3, 0x1c, 0xf0,  0x86, 0x6a, 0x6b, 0x0f,
    0x03, 0x2d, 0x86,  0x17, 0x7b, 0x10,  0xf6, 0x80, 0x78, 0x7a,  0xa1, 0xe1, 0xef, 0x8c,  0xf6, 0x87, 0x4b, 0xa7,
    0xe2, 0x77, 0xfa,  0xb8, 0x81, 0xee,  0x77, 0xc0, 0x9d, 0x29,  0x20, 0x27, 0x71, 0x12,  0xe0, 0x6b, 0xd1, 0x7c,
    0x0a, 0x89, 0x7d,  0x87, 0xc4, 0x101, 0xc1, 0x31, 0xaf, 0x38,  0x03, 0x68, 0x1b, 0x76,  0x79, 0x3f, 0xdb, 0xc7,
    0x1b, 0x36, 0x7b,  0xe2, 0x63, 0x81,  0xee, 0x0c, 0x63, 0x8b,  0x78, 0x38, 0x97, 0x9b,  0xd7, 0x8f, 0xdd, 0xf2,
    0xa3, 0x77, 0x8c,  0xc3, 0x39, 0x20,  0xb3, 0x12, 0x11, 0x0e,  0x17, 0x42, 0x80, 0x2c,  0xc4, 0x92, 0x59, 0xc8,
    0xdb, 0x40, 0x76,  0x64, 0xb4, 0x55,  0x1a, 0x9e, 0xfe, 0x5f,  0x06, 0x3c, 0x41, 0xef,  0xd4, 0xaa, 0x98, 0x29,
    0xcd, 0x1f, 0x02,  0xa8, 0x87, 0xd2,  0xa0, 0x93, 0x98, 0xef,  0x0c, 0x43, 0xed, 0x9d,  0xc2, 0xeb, 0x81, 0xe9,
    0x64, 0x23, 0x68,  0x1e, 0x25, 0x57,  0xde, 0x9a, 0xcf, 0x7f,  0xe5, 0xba, 0x41, 0xea,  0xea, 0x36, 0x1a, 0x28,
    0x79, 0x20, 0x5e,  0x18, 0x4e, 0x7c,  0x8e, 0x58, 0x7a, 0xef,  0x91, 0x02, 0x93, 0xbb,  0x56, 0xa1, 0x49, 0x1b,
    0x79, 0x92, 0xf3,  0x58, 0x4f, 0x52,  0x9c, 0x02, 0x77, 0xaf,  0x2a, 0x8f, 0x49, 0xd0,  0x99, 0x4d, 0x98, 0x101,
    0x60, 0x93, 0x100, 0x75, 0x31, 0xce,  0x49, 0x20, 0x56, 0x57,  0xe2, 0xf5, 0x26, 0x2b,  0x8a, 0xbf, 0xde, 0xd0,
    0x83, 0x34, 0xf4,  0x17};

/* CRC32 with polynomial 0xEDB88320 */
static uint32_t arsenic_crc_table[256];
static int      arsenic_crc_initialized = 0;

static void arsenic_init_crc(void)
{
    if(arsenic_crc_initialized) return;
    for(uint32_t i = 0; i < 256; i++)
    {
        uint32_t c = i;
        for(int j = 0; j < 8; j++) c = (c >> 1) ^ (c & 1 ? 0xEDB88320u : 0);
        arsenic_crc_table[i] = c;
    }
    arsenic_crc_initialized = 1;
}

int stuffit_arsenic_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    arsenic_init_crc();

    ArsenicDecoder dec;
    arsenic_init(&dec, src, src_size);

    ArithModel initial, selector, mtfmodels[7];
    arith_model_init(&initial, 0, 1, 1, 256);
    arith_model_init(&selector, 0, 10, 8, 1024);
    arith_model_init(&mtfmodels[0], 2, 3, 8, 1024);
    arith_model_init(&mtfmodels[1], 4, 7, 4, 1024);
    arith_model_init(&mtfmodels[2], 8, 15, 4, 1024);
    arith_model_init(&mtfmodels[3], 16, 31, 4, 1024);
    arith_model_init(&mtfmodels[4], 32, 63, 2, 1024);
    arith_model_init(&mtfmodels[5], 64, 127, 2, 1024);
    arith_model_init(&mtfmodels[6], 128, 255, 1, 1024);

    /* Verify "As" marker */
    if(arsenic_next_bitstring(&dec, &initial, 8) != 'A') return -1;
    if(arsenic_next_bitstring(&dec, &initial, 8) != 's') return -1;

    int blockbits = arsenic_next_bitstring(&dec, &initial, 4) + 9;
    int blocksize = 1 << blockbits;

    uint8_t  *block     = malloc(blocksize);
    uint32_t *transform = malloc(blocksize * sizeof(uint32_t));
    if(!block || !transform)
    {
        free(block);
        free(transform);
        return -1;
    }

    uint32_t crc           = 0xffffffff;
    int      end_of_blocks = arsenic_next_symbol(&dec, &initial);

    int last = 0, count = 0, repeat = 0;
    int numbytes = 0, bytecount = 0, transformindex = 0;
    int randomized = 0, randindex = 0, randcount = 0;

    while(di < limit)
    {
        if(repeat > 0)
        {
            repeat--;
            uint8_t b = (uint8_t)last;
            crc       = (crc >> 8) ^ arsenic_crc_table[(crc ^ b) & 0xff];
            dst[di++] = b;
            continue;
        }

    retry:
        if(bytecount >= numbytes)
        {
            if(end_of_blocks) break;

            /* Read a new block */
            StuffitMTFState mtf;
            stuffit_reset_mtf(&mtf);

            randomized     = arsenic_next_symbol(&dec, &initial);
            transformindex = arsenic_next_bitstring(&dec, &initial, blockbits);
            numbytes       = 0;

            for(;;)
            {
                int sel = arsenic_next_symbol(&dec, &selector);
                if(sel == 0 || sel == 1)
                {
                    int zerostate = 1, zerocount = 0;
                    while(sel < 2)
                    {
                        if(sel == 0)
                            zerocount += zerostate;
                        else
                            zerocount += 2 * zerostate;
                        zerostate *= 2;
                        sel = arsenic_next_symbol(&dec, &selector);
                    }
                    if(numbytes + zerocount > blocksize)
                    {
                        free(block);
                        free(transform);
                        return -1;
                    }
                    memset(&block[numbytes], stuffit_decode_mtf(&mtf, 0), zerocount);
                    numbytes += zerocount;
                }

                int symbol;
                if(sel == 10)
                    break;
                else if(sel == 2)
                    symbol = 1;
                else
                    symbol = arsenic_next_symbol(&dec, &mtfmodels[sel - 3]);

                if(numbytes >= blocksize)
                {
                    free(block);
                    free(transform);
                    return -1;
                }
                block[numbytes++] = stuffit_decode_mtf(&mtf, symbol);
            }

            if(transformindex >= numbytes)
            {
                free(block);
                free(transform);
                return -1;
            }

            arith_model_reset(&selector);
            for(int i = 0; i < 7; i++) arith_model_reset(&mtfmodels[i]);

            if(arsenic_next_symbol(&dec, &initial))
            {
                /* comp CRC */
                arsenic_next_bitstring(&dec, &initial, 32);
                end_of_blocks = 1;
            }

            stuffit_calculate_inverse_bwt(transform, block, numbytes);

            bytecount = 0;
            count     = 0;
            last      = 0;
            randindex = 0;
            randcount = arsenic_rand_table[0];
        }

        transformindex = transform[transformindex];
        int byte       = block[transformindex];

        if(randomized && randcount == bytecount)
        {
            byte ^= 1;
            randindex = (randindex + 1) & 255;
            randcount += arsenic_rand_table[randindex];
        }

        bytecount++;

        if(count == 4)
        {
            count = 0;
            if(byte == 0) goto retry;
            repeat    = byte - 1;
            uint8_t b = (uint8_t)last;
            crc       = (crc >> 8) ^ arsenic_crc_table[(crc ^ b) & 0xff];
            if(di < limit) dst[di++] = b;
        }
        else
        {
            if(byte == last)
                count++;
            else
            {
                count = 1;
                last  = byte;
            }

            uint8_t b = (uint8_t)byte;
            crc       = (crc >> 8) ^ arsenic_crc_table[(crc ^ b) & 0xff];
            if(di < limit) dst[di++] = b;
        }
    }

    free(block);
    free(transform);
    *dst_size = di;
    return 0;
}
