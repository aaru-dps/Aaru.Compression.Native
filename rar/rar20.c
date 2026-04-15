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
 * RAR 2.0 decompressor (UNP_VER=20).
 *
 * Block-based dynamic Huffman LZ77 with optional multichannel audio mode.
 * 1MB (0x100000) sliding window.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bitstream.h"
#include "filters.h"
#include "huffman.h"
#include "rar.h"

#define RAR20_WINDOW_SIZE 0x100000 /* 1 MB */
#define RAR20_WINDOW_MASK (RAR20_WINDOW_SIZE - 1)

#define RAR20_MAINCODE_SIZE    298
#define RAR20_OFFSETCODE_SIZE  48
#define RAR20_LENGTHCODE_SIZE  28
#define RAR20_AUDIOCODE_SIZE   257
#define RAR20_PRECODE_SIZE     19
#define RAR20_LENGTHTABLE_SIZE 1028

static const int lengthbases[28] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  10,  12,  14,  16,  20,
                                    24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224};

static const int lengthbits[28] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};

static const int offsetbases[48] = {0,      1,      2,      3,      4,      6,      8,      12,     16,     24,
                                    32,     48,     64,     96,     128,    192,    256,    384,    512,    768,
                                    1024,   1536,   2048,   3072,   4096,   6144,   8192,   12288,  16384,  24576,
                                    32768,  49152,  65536,  98304,  131072, 196608, 262144, 327680, 393216, 458752,
                                    524288, 589824, 655360, 720896, 786432, 851968, 917504, 983040};

static const int offsetbits[48] = {0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
                                   7,  7,  8,  8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
                                   15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

static const int shortbases[8] = {0, 4, 8, 16, 32, 64, 128, 192};
static const int shortbits[8]  = {2, 2, 3, 4, 5, 6, 6, 6};

/**
 * Parse the 19-symbol precode and rebuild the Huffman tables from the
 * cumulative length table. Sets audioblock / numchannels as appropriate.
 *
 * @return 0 on success, -1 on error.
 */
static int alloc_and_parse_codes(rar_bitstream_t *bs, rar_huff_code_t *maincode, rar_huff_code_t *offsetcode,
                                 rar_huff_code_t *lengthcode, rar_huff_code_t audiocode[4],
                                 int lengthtable[RAR20_LENGTHTABLE_SIZE], int *audioblock, int *numchannels,
                                 int *channel)
{
    int ret;

    /* Free previous codes */
    rar_huff_free(maincode);
    rar_huff_free(offsetcode);
    rar_huff_free(lengthcode);
    for(int i = 0; i < 4; i++) rar_huff_free(&audiocode[i]);

    *audioblock = rar_bs_read_bit(bs);

    if(rar_bs_read_bit(bs) == 0) memset(lengthtable, 0, RAR20_LENGTHTABLE_SIZE * sizeof(int));

    int count;
    if(*audioblock)
    {
        *numchannels = (int)rar_bs_read_bits(bs, 2) + 1;
        count        = *numchannels * RAR20_AUDIOCODE_SIZE;
        if(*channel >= *numchannels) *channel = 0;
    }
    else
    {
        count = RAR20_MAINCODE_SIZE + RAR20_OFFSETCODE_SIZE + RAR20_LENGTHCODE_SIZE;
    }

    /* Build precode (19 symbols, 4-bit lengths) */
    int prelengths[RAR20_PRECODE_SIZE];
    for(int i = 0; i < RAR20_PRECODE_SIZE; i++) prelengths[i] = (int)rar_bs_read_bits(bs, 4);

    rar_huff_code_t precode;
    memset(&precode, 0, sizeof(precode));
    ret = rar_huff_create_from_lengths(&precode, prelengths, RAR20_PRECODE_SIZE, RAR_HUFF_MAX_CODE_LEN);
    if(ret != 0) return -1;

    /* Decode length table using precode */
    int i = 0;
    while(i < count)
    {
        int val = rar_huff_decode(&precode, bs);
        if(val < 0)
        {
            rar_huff_free(&precode);
            return -1;
        }

        if(val < 16)
        {
            lengthtable[i] = (lengthtable[i] + val) & 0x0f;
            i++;
        }
        else if(val == 16)
        {
            /* Repeat previous length */
            if(i == 0)
            {
                rar_huff_free(&precode);
                return -1;
            }
            int n = (int)rar_bs_read_bits(bs, 2) + 3;
            for(int j = 0; j < n && i < count; j++)
            {
                lengthtable[i] = lengthtable[i - 1];
                i++;
            }
        }
        else
        {
            /* Zero run: val==17 → short, val==18 → long */
            int n;
            if(val == 17)
                n = (int)rar_bs_read_bits(bs, 3) + 3;
            else
                n = (int)rar_bs_read_bits(bs, 7) + 11;

            for(int j = 0; j < n && i < count; j++) lengthtable[i++] = 0;
        }
    }

    rar_huff_free(&precode);

    /* Build the actual Huffman codes from the length table */
    if(*audioblock)
    {
        for(int ch = 0; ch < *numchannels; ch++)
        {
            ret = rar_huff_create_from_lengths(&audiocode[ch], &lengthtable[ch * RAR20_AUDIOCODE_SIZE],
                                               RAR20_AUDIOCODE_SIZE, RAR_HUFF_MAX_CODE_LEN);
            if(ret != 0) return -1;
        }
    }
    else
    {
        ret = rar_huff_create_from_lengths(maincode, &lengthtable[0], RAR20_MAINCODE_SIZE, RAR_HUFF_MAX_CODE_LEN);
        if(ret != 0) return -1;

        ret = rar_huff_create_from_lengths(offsetcode, &lengthtable[RAR20_MAINCODE_SIZE], RAR20_OFFSETCODE_SIZE,
                                           RAR_HUFF_MAX_CODE_LEN);
        if(ret != 0) return -1;

        ret = rar_huff_create_from_lengths(lengthcode, &lengthtable[RAR20_MAINCODE_SIZE + RAR20_OFFSETCODE_SIZE],
                                           RAR20_LENGTHCODE_SIZE, RAR_HUFF_MAX_CODE_LEN);
        if(ret != 0) return -1;
    }

    return 0;
}

AARU_EXPORT int AARU_CALL rar20_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    if(!in_buf || !out_buf || !out_len || *out_len == 0) return -1;

    const size_t dest_size = *out_len;

    rar_bitstream_t bs;
    rar_bs_init(&bs, in_buf, in_len);

    rar_huff_code_t maincode, offsetcode, lengthcode;
    rar_huff_code_t audiocode[4];
    memset(&maincode, 0, sizeof(maincode));
    memset(&offsetcode, 0, sizeof(offsetcode));
    memset(&lengthcode, 0, sizeof(lengthcode));
    memset(audiocode, 0, sizeof(audiocode));

    int lengthtable[RAR20_LENGTHTABLE_SIZE];
    memset(lengthtable, 0, sizeof(lengthtable));

    int lastoffset = 0, lastlength = 0;
    int oldoffset[4]   = {0, 0, 0, 0};
    int oldoffsetindex = 0;

    int                 audioblock   = 0;
    int                 channel      = 0;
    int                 channeldelta = 0;
    int                 numchannels  = 0;
    rar_audio20_state_t audiostate[4];
    memset(audiostate, 0, sizeof(audiostate));

    /* Parse initial code tables */
    if(alloc_and_parse_codes(&bs, &maincode, &offsetcode, &lengthcode, audiocode, lengthtable, &audioblock,
                             &numchannels, &channel) != 0)
        goto error;

    size_t pos = 0;
    int    ret = 0;

    while(pos < dest_size)
    {
        if(audioblock)
        {
            int symbol = rar_huff_decode(&audiocode[channel], &bs);
            if(symbol < 0) goto error;

            if(symbol == 256)
            {
                /* New block — rebuild tables */
                if(alloc_and_parse_codes(&bs, &maincode, &offsetcode, &lengthcode, audiocode, lengthtable, &audioblock,
                                         &numchannels, &channel) != 0)
                    goto error;
                continue;
            }

            int byte       = rar_audio20_decode(&audiostate[channel], &channeldelta, symbol);
            out_buf[pos++] = (uint8_t)(byte & 0xff);

            channel++;
            if(channel >= numchannels) channel = 0;
        }
        else
        {
            int symbol = rar_huff_decode(&maincode, &bs);
            if(symbol < 0) goto error;

            if(symbol < 256)
            {
                /* Literal byte */
                out_buf[pos++] = (uint8_t)symbol;
                continue;
            }

            int offs, len;

            if(symbol == 256)
            {
                /* Repeat last match */
                offs = lastoffset;
                len  = lastlength;
            }
            else if(symbol <= 260)
            {
                /* Old offset reuse (4-slot ring) */
                offs = oldoffset[(oldoffsetindex - (symbol - 256)) & 3];

                int lensymbol = rar_huff_decode(&lengthcode, &bs);
                if(lensymbol < 0) goto error;

                len = lengthbases[lensymbol] + 2;
                if(lengthbits[lensymbol] > 0) len += (int)rar_bs_read_bits(&bs, lengthbits[lensymbol]);

                /* Distance-dependent length bonus */
                if(offs >= 0x40000) len++;
                if(offs >= 0x2000) len++;
                if(offs >= 0x101) len++;
            }
            else if(symbol <= 268)
            {
                /* Short match (length=2) */
                int idx = symbol - 261;
                offs    = shortbases[idx] + 1;
                if(shortbits[idx] > 0) offs += (int)rar_bs_read_bits(&bs, shortbits[idx]);
                len = 2;
            }
            else if(symbol == 269)
            {
                /* New block — rebuild tables */
                if(alloc_and_parse_codes(&bs, &maincode, &offsetcode, &lengthcode, audiocode, lengthtable, &audioblock,
                                         &numchannels, &channel) != 0)
                    goto error;
                continue;
            }
            else
            {
                /* Full match (symbol >= 270) */
                int lidx = symbol - 270;
                if(lidx >= RAR20_LENGTHCODE_SIZE) goto error;

                len = lengthbases[lidx] + 3;
                if(lengthbits[lidx] > 0) len += (int)rar_bs_read_bits(&bs, lengthbits[lidx]);

                int offsymbol = rar_huff_decode(&offsetcode, &bs);
                if(offsymbol < 0) goto error;

                offs = offsetbases[offsymbol] + 1;
                if(offsetbits[offsymbol] > 0) offs += (int)rar_bs_read_bits(&bs, offsetbits[offsymbol]);

                /* Distance-dependent length bonus */
                if(offs >= 0x40000) len++;
                if(offs >= 0x2000) len++;
            }

            /* Update offset history ring */
            lastoffset                      = offs;
            lastlength                      = len;
            oldoffset[oldoffsetindex++ & 3] = offs;

            /* Copy match bytes */
            if(offs <= 0 || (size_t)offs > pos) goto error;
            for(int k = 0; k < len && pos < dest_size; k++)
            {
                out_buf[pos] = out_buf[pos - offs];
                pos++;
            }
        }
    }

    *out_len = pos;
    ret      = 0;
    goto cleanup;

error:
    ret = -1;

cleanup:
    rar_huff_free(&maincode);
    rar_huff_free(&offsetcode);
    rar_huff_free(&lengthcode);
    for(int i = 0; i < 4; i++) rar_huff_free(&audiocode[i]);

    return ret;
}
