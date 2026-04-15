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
 * RAR 5.0 decompressor.
 *
 * Variable-size LZSS window (power of 2, caller-specified).
 * Block-based Huffman coding with 4 code tables and 4 native filter types.
 */

#include <stdlib.h>
#include <string.h>

#include "bitstream.h"
#include "filters.h"
#include "huffman.h"
#include "lzss.h"
#include "rar.h"

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define RAR50_MAIN_SYMBOLS    306
#define RAR50_OFFSET_SYMBOLS  64
#define RAR50_LOWOFF_SYMBOLS  16
#define RAR50_LENGTH_SYMBOLS  44
#define RAR50_TOTAL_SYMBOLS   (RAR50_MAIN_SYMBOLS + RAR50_OFFSET_SYMBOLS + RAR50_LOWOFF_SYMBOLS + RAR50_LENGTH_SYMBOLS)
#define RAR50_PRECODE_SYMBOLS 20

/* ------------------------------------------------------------------ */
/* Context                                                             */
/* ------------------------------------------------------------------ */

typedef struct
{
    rar_bitstream_t bs;
    rar_lzss_t      lzss;

    rar_huff_code_t maincode;
    rar_huff_code_t offsetcode;
    rar_huff_code_t lowoffsetcode;
    rar_huff_code_t lengthcode;
    int             codes_valid;

    int lengthtable[RAR50_TOTAL_SYMBOLS];

    int lastlength;
    int oldoffset[4];

    size_t blockbitend;
    int    islastblock;

    uint8_t *out_buf;
    size_t   out_size; /* expected output size */
    size_t   out_pos;  /* bytes written to out_buf */

    rar_filter50_t *filters; /* linked list head */
} rar50_ctx_t;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int read_length_with_symbol(rar_bitstream_t *bs, int symbol)
{
    if(symbol < 8) return symbol + 2;

    int lenbits = symbol / 4 - 1;
    int length  = ((4 + (symbol & 3)) << lenbits) + 2;
    length += (int)rar_bs_read_bits(bs, lenbits);
    return length;
}

static uint32_t read_filter_integer(rar_bitstream_t *bs)
{
    int      count = (int)rar_bs_read_bits(bs, 2) + 1;
    uint32_t value = 0;
    for(int i = 0; i < count; i++) value += rar_bs_read_bits(bs, 8) << (i * 8);
    return value;
}

/* ------------------------------------------------------------------ */
/* Filter list management                                              */
/* ------------------------------------------------------------------ */

static void append_filter(rar50_ctx_t *ctx, rar_filter50_t *f)
{
    if(!ctx->filters)
    {
        ctx->filters = f;
        return;
    }
    rar_filter50_t *p = ctx->filters;
    while(p->next) p = p->next;
    p->next = f;
}

/* ------------------------------------------------------------------ */
/* Flush: copy LZSS data to out_buf, applying filters as needed        */
/* ------------------------------------------------------------------ */

static int flush_output(rar50_ctx_t *ctx, int64_t flush_end)
{
    while(ctx->out_pos < ctx->out_size && (int64_t)ctx->out_pos < flush_end)
    {
        /* Check if a filter is pending at the current output position */
        if(ctx->filters && ctx->filters->start == (int64_t)ctx->out_pos)
        {
            rar_filter50_t *f    = ctx->filters;
            size_t          flen = f->length;

            /* Sanity check */
            if(ctx->out_pos + flen > ctx->out_size) return -1;

            /* Allocate temp buffer and copy from LZSS window */
            uint8_t *tmp = (uint8_t *)malloc(flen);
            if(!tmp) return -1;

            rar_lzss_copy_bytes(&ctx->lzss, tmp, (int64_t)ctx->out_pos, flen);

            /* Apply filter in-place on tmp */
            rar_filter50_execute(f, tmp, flen, (int64_t)ctx->out_pos);

            /* Copy filtered data to output */
            memcpy(ctx->out_buf + ctx->out_pos, tmp, flen);
            ctx->out_pos += flen;

            free(tmp);

            /* Remove filter from list */
            ctx->filters = f->next;
            f->next      = NULL;
            rar_filter50_free(f);
        }
        else
        {
            /* Determine how many bytes to copy without hitting a filter */
            int64_t copy_end = flush_end;
            if(ctx->filters && ctx->filters->start < copy_end) copy_end = ctx->filters->start;

            /* Also don't cross LZSS window boundary */
            int64_t win_edge = rar_lzss_next_window_edge(&ctx->lzss, (int64_t)ctx->out_pos);
            if(copy_end > win_edge) copy_end = win_edge;

            if(copy_end > (int64_t)ctx->out_size) copy_end = (int64_t)ctx->out_size;

            if(copy_end <= (int64_t)ctx->out_pos) break;

            size_t chunk = (size_t)(copy_end - (int64_t)ctx->out_pos);
            rar_lzss_copy_bytes(&ctx->lzss, ctx->out_buf + ctx->out_pos, (int64_t)ctx->out_pos, chunk);
            ctx->out_pos += chunk;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Block header                                                        */
/* ------------------------------------------------------------------ */

static int read_block_header(rar50_ctx_t *ctx);
static int alloc_and_parse_codes(rar50_ctx_t *ctx);

static int read_block_header(rar50_ctx_t *ctx)
{
    rar_bs_skip_to_byte_boundary(&ctx->bs);

    int checksum = 0x5a;

    int flags = (int)rar_bs_read_byte(&ctx->bs);
    checksum ^= flags;

    int sizecount = ((flags >> 3) & 3) + 1;
    if(sizecount == 4) return -1;

    int blockbitsize = (flags & 7) + 1;

    int correctchecksum = (int)rar_bs_read_byte(&ctx->bs);

    uint32_t blocksize = 0;
    for(int i = 0; i < sizecount; i++)
    {
        int byte = (int)rar_bs_read_byte(&ctx->bs);
        blocksize += (uint32_t)byte << (i * 8);
        checksum ^= byte;
    }

    if((checksum & 0xff) != (correctchecksum & 0xff)) return -1;

    ctx->blockbitend = rar_bs_bit_offset(&ctx->bs) + (size_t)blocksize * 8 + (size_t)blockbitsize - 8;
    ctx->islastblock = (flags & 0x40) != 0;

    if(flags & 0x80)
    {
        if(alloc_and_parse_codes(ctx) != 0) return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Huffman table construction                                          */
/* ------------------------------------------------------------------ */

static int alloc_and_parse_codes(rar50_ctx_t *ctx)
{
    /* Free previous codes if any */
    if(ctx->codes_valid)
    {
        rar_huff_free(&ctx->maincode);
        rar_huff_free(&ctx->offsetcode);
        rar_huff_free(&ctx->lowoffsetcode);
        rar_huff_free(&ctx->lengthcode);
        ctx->codes_valid = 0;
    }

    /* Read precode lengths */
    int prelengths[RAR50_PRECODE_SYMBOLS];
    for(int i = 0; i < RAR50_PRECODE_SYMBOLS;)
    {
        int length = (int)rar_bs_read_bits(&ctx->bs, 4);
        if(length == 15)
        {
            int count = (int)rar_bs_read_bits(&ctx->bs, 4) + 2;
            if(count == 2) { prelengths[i++] = 15; }
            else
            {
                for(int j = 0; j < count && i < RAR50_PRECODE_SYMBOLS; j++) prelengths[i++] = 0;
            }
        }
        else
        {
            prelengths[i++] = length;
        }
    }

    rar_huff_code_t precode;
    if(rar_huff_create_from_lengths(&precode, prelengths, RAR50_PRECODE_SYMBOLS, RAR_HUFF_MAX_CODE_LEN) != 0) return -1;

    /* Decode length table using precode */
    for(int i = 0; i < RAR50_TOTAL_SYMBOLS;)
    {
        int val = rar_huff_decode(&precode, &ctx->bs);
        if(val < 0)
        {
            rar_huff_free(&precode);
            return -1;
        }

        if(val < 16) { ctx->lengthtable[i++] = val; }
        else if(val < 18)
        {
            /* 16 = repeat previous (3+3), 17 = repeat previous (7+11) */
            if(i == 0)
            {
                rar_huff_free(&precode);
                return -1;
            }
            int n;
            if(val == 16)
                n = (int)rar_bs_read_bits(&ctx->bs, 3) + 3;
            else
                n = (int)rar_bs_read_bits(&ctx->bs, 7) + 11;

            int fill = ctx->lengthtable[i - 1];
            for(int j = 0; j < n && i < RAR50_TOTAL_SYMBOLS; j++) ctx->lengthtable[i++] = fill;
        }
        else /* val == 18 or 19: zero run */
        {
            int n;
            if(val == 18)
                n = (int)rar_bs_read_bits(&ctx->bs, 3) + 3;
            else
                n = (int)rar_bs_read_bits(&ctx->bs, 7) + 11;

            for(int j = 0; j < n && i < RAR50_TOTAL_SYMBOLS; j++) ctx->lengthtable[i++] = 0;
        }
    }

    rar_huff_free(&precode);

    /* Build the 4 Huffman codes from the length table */
    int off = 0;

    if(rar_huff_create_from_lengths(&ctx->maincode, &ctx->lengthtable[off], RAR50_MAIN_SYMBOLS,
                                    RAR_HUFF_MAX_CODE_LEN) != 0)
        return -1;
    off += RAR50_MAIN_SYMBOLS;

    if(rar_huff_create_from_lengths(&ctx->offsetcode, &ctx->lengthtable[off], RAR50_OFFSET_SYMBOLS,
                                    RAR_HUFF_MAX_CODE_LEN) != 0)
    {
        rar_huff_free(&ctx->maincode);
        return -1;
    }
    off += RAR50_OFFSET_SYMBOLS;

    if(rar_huff_create_from_lengths(&ctx->lowoffsetcode, &ctx->lengthtable[off], RAR50_LOWOFF_SYMBOLS,
                                    RAR_HUFF_MAX_CODE_LEN) != 0)
    {
        rar_huff_free(&ctx->maincode);
        rar_huff_free(&ctx->offsetcode);
        return -1;
    }
    off += RAR50_LOWOFF_SYMBOLS;

    if(rar_huff_create_from_lengths(&ctx->lengthcode, &ctx->lengthtable[off], RAR50_LENGTH_SYMBOLS,
                                    RAR_HUFF_MAX_CODE_LEN) != 0)
    {
        rar_huff_free(&ctx->maincode);
        rar_huff_free(&ctx->offsetcode);
        rar_huff_free(&ctx->lowoffsetcode);
        return -1;
    }

    ctx->codes_valid = 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Main expansion loop                                                 */
/* ------------------------------------------------------------------ */

static int expand_to_position(rar50_ctx_t *ctx, int64_t end)
{
    for(;;)
    {
        if(ctx->lzss.position >= end) return 0;

        /* Check for block boundary */
        while(rar_bs_bit_offset(&ctx->bs) >= ctx->blockbitend)
        {
            if(ctx->islastblock)
            {
                /* No more blocks — return what we have */
                return 0;
            }
            if(read_block_header(ctx) != 0) return -1;
        }

        if(!ctx->codes_valid) return -1;

        int symbol = rar_huff_decode(&ctx->maincode, &ctx->bs);
        if(symbol < 0) return -1;

        int offs, len;

        if(symbol < 256)
        {
            /* Literal byte */
            rar_lzss_emit_literal(&ctx->lzss, (uint8_t)symbol);
            continue;
        }
        else if(symbol == 256)
        {
            /* Filter */
            int64_t  start  = (int64_t)read_filter_integer(&ctx->bs) + ctx->lzss.position;
            uint32_t length = read_filter_integer(&ctx->bs);
            int      type   = (int)rar_bs_read_bits(&ctx->bs, 3);

            int channels = 0;
            if(type == RAR5_FILTER_DELTA)
                channels = (int)rar_bs_read_bits(&ctx->bs, 5) + 1;
            else if(type > RAR5_FILTER_ARM)
                return -1; /* unsupported filter type */

            rar_filter50_t *f = rar_filter50_create(start, length, type, channels);
            if(!f) return -1;
            append_filter(ctx, f);

            /* Stop expansion before the filter start */
            if(end > start) end = start;
            continue;
        }
        else if(symbol == 257)
        {
            /* Repeat last match */
            if(ctx->lastlength == 0) continue;

            offs = ctx->oldoffset[0];
            len  = ctx->lastlength;
        }
        else if(symbol < 262)
        {
            /* Reuse old offset */
            int offsindex = symbol - 258;
            offs          = ctx->oldoffset[offsindex];

            int lensymbol = rar_huff_decode(&ctx->lengthcode, &ctx->bs);
            if(lensymbol < 0) return -1;
            len = read_length_with_symbol(&ctx->bs, lensymbol);

            /* Shift old offsets down */
            for(int i = offsindex; i > 0; i--) ctx->oldoffset[i] = ctx->oldoffset[i - 1];
            ctx->oldoffset[0] = offs;
        }
        else /* symbol >= 262 */
        {
            /* Full match: length + offset */
            len = read_length_with_symbol(&ctx->bs, symbol - 262);

            int offssymbol = rar_huff_decode(&ctx->offsetcode, &ctx->bs);
            if(offssymbol < 0) return -1;

            if(offssymbol < 4) { offs = offssymbol + 1; }
            else
            {
                int offsbits = offssymbol / 2 - 1;
                int offslow;

                if(offsbits >= 4)
                {
                    if(offsbits > 4)
                        offslow = (int)rar_bs_read_bits(&ctx->bs, offsbits - 4) << 4;
                    else
                        offslow = 0;
                    int lowsym = rar_huff_decode(&ctx->lowoffsetcode, &ctx->bs);
                    if(lowsym < 0) return -1;
                    offslow += lowsym;
                }
                else
                {
                    offslow = (int)rar_bs_read_bits(&ctx->bs, offsbits);
                }

                offs = ((2 + (offssymbol & 1)) << offsbits) + offslow + 1;
            }

            /* Distance-dependent length bonus */
            if(offs > 0x40000) len++;
            if(offs > 0x2000) len++;
            if(offs > 0x100) len++;

            /* Push new offset into ring */
            for(int i = 3; i > 0; i--) ctx->oldoffset[i] = ctx->oldoffset[i - 1];
            ctx->oldoffset[0] = offs;
        }

        ctx->lastlength = len;
        rar_lzss_emit_match(&ctx->lzss, offs, len);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int rar50_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len, size_t window_size)
{
    if(!in_buf || !out_buf || !out_len || *out_len == 0 || window_size == 0) return -1;

    /* window_size must be a power of 2 */
    if(window_size & (window_size - 1)) return -1;

    rar50_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    rar_bs_init(&ctx.bs, in_buf, in_len);

    if(rar_lzss_init(&ctx.lzss, window_size) != 0) return -1;

    ctx.out_buf     = out_buf;
    ctx.out_size    = *out_len;
    ctx.out_pos     = 0;
    ctx.filters     = NULL;
    ctx.codes_valid = 0;

    ctx.lastlength = 0;
    memset(ctx.oldoffset, 0, sizeof(ctx.oldoffset));
    memset(ctx.lengthtable, 0, sizeof(ctx.lengthtable));

    ctx.blockbitend = 0;
    ctx.islastblock = 0;

    /* Read the first block header */
    if(read_block_header(&ctx) != 0) goto fail;

    /* Main decompression loop */
    while(ctx.out_pos < ctx.out_size)
    {
        int64_t target = (int64_t)ctx.out_size;

        /* If a filter is pending, stop expansion at its start */
        if(ctx.filters) target = ctx.filters->start;

        /* Also limit to out_size */
        if(target > (int64_t)ctx.out_size) target = (int64_t)ctx.out_size;

        /* Expand LZSS data up to target */
        if(expand_to_position(&ctx, target) != 0) goto fail;

        /* Flush available data to out_buf (applying filters) */
        if(flush_output(&ctx, ctx.lzss.position) != 0) goto fail;

        /* If we're stuck (no progress), either we finished or there's an error */
        if(ctx.lzss.position <= (int64_t)ctx.out_pos && ctx.islastblock &&
           rar_bs_bit_offset(&ctx.bs) >= ctx.blockbitend)
            break;
    }

    *out_len = ctx.out_pos;

    /* Cleanup */
    if(ctx.codes_valid)
    {
        rar_huff_free(&ctx.maincode);
        rar_huff_free(&ctx.offsetcode);
        rar_huff_free(&ctx.lowoffsetcode);
        rar_huff_free(&ctx.lengthcode);
    }
    rar_lzss_cleanup(&ctx.lzss);
    rar_filter50_free_chain(ctx.filters);

    return 0;

fail:
    if(ctx.codes_valid)
    {
        rar_huff_free(&ctx.maincode);
        rar_huff_free(&ctx.offsetcode);
        rar_huff_free(&ctx.lowoffsetcode);
        rar_huff_free(&ctx.lengthcode);
    }
    rar_lzss_cleanup(&ctx.lzss);
    rar_filter50_free_chain(ctx.filters);

    return -1;
}
