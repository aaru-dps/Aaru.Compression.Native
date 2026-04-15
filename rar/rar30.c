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
 * RAR 3.0 (UNP_VER=29) decompressor.
 *
 * 4 MB LZSS window, dual-mode blocks (Huffman LZ77 or PPMd Variant H),
 * VM-based post-decompression filters.
 */

#include <stdlib.h>
#include <string.h>

#include "../ppmd/SubAllocatorVariantH.h"
#include "../ppmd/VariantH.h"
#include "bitstream.h"
#include "filters.h"
#include "huffman.h"
#include "lzss.h"
#include "rar.h"
#include "vm.h"

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define RAR30_WINDOW_SIZE   0x400000 /* 4 MB */
#define RAR30_NUM_MAIN      299
#define RAR30_NUM_OFFSET    60
#define RAR30_NUM_LOWOFFSET 17
#define RAR30_NUM_LENGTH    28
#define RAR30_TABLE_SIZE    (RAR30_NUM_MAIN + RAR30_NUM_OFFSET + RAR30_NUM_LOWOFFSET + RAR30_NUM_LENGTH)

#define RAR30_MAX_FILTERS 1024

static const int lengthbases[28] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  10,  12,  14,  16,  20,
                                    24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224};

static const int lengthbits[28] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5};

static const int offsetbases[60] = {
    0,       1,       2,       3,       4,       6,       8,       12,      16,      24,      32,      48,
    64,      96,      128,     192,     256,     384,     512,     768,     1024,    1536,    2048,    3072,
    4096,    6144,    8192,    12288,   16384,   24576,   32768,   49152,   65536,   98304,   131072,  196608,
    262144,  327680,  393216,  458752,  524288,  589824,  655360,  720896,  786432,  851968,  917504,  983040,
    1048576, 1310720, 1572864, 1835008, 2097152, 2359296, 2621440, 2883584, 3145728, 3407872, 3670016, 3932160};

static const int offsetbits[60] = {0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,
                                   9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 16, 16, 16, 16,
                                   16, 16, 16, 16, 16, 16, 16, 16, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18};

static const int shortbases[8] = {0, 4, 8, 16, 32, 64, 128, 192};
static const int shortbits[8]  = {2, 2, 3, 4, 5, 6, 6, 6};

/* ------------------------------------------------------------------ */
/* PPMd byte-read callback for RAR bitstream                           */
/* ------------------------------------------------------------------ */

static int ppmd_read_byte(void *context)
{
    rar_bitstream_t *bs = (rar_bitstream_t *)context;
    if(bs->byte_pos >= bs->len && bs->bits_left < 8) return -1;
    return (int)rar_bs_read_byte(bs);
}

/* ------------------------------------------------------------------ */
/* Decoder context                                                     */
/* ------------------------------------------------------------------ */

typedef struct
{
    rar_bitstream_t bs;
    rar_lzss_t      lzss;

    /* Huffman codes */
    rar_huff_code_t maincode;
    rar_huff_code_t offsetcode;
    rar_huff_code_t lowoffsetcode;
    rar_huff_code_t lengthcode;
    int             codes_valid;

    /* Cumulative code lengths table (persists across blocks) */
    int lengthtable[RAR30_TABLE_SIZE];

    /* Match history */
    int lastoffset, lastlength;
    int oldoffset[4];
    int lastlowoffset, numlowoffsetrepeats;

    /* PPMd state */
    int                       ppmblock;
    PPMdModelVariantH         ppmd;
    PPMdSubAllocatorVariantH *alloc;
    int                       ppmescape;
    int                       ppmd_initialized;

    /* VM filter state */
    rar_vm_t          vm;
    rar_vm_program_t *filtercode[RAR30_MAX_FILTERS];
    int               num_filtercodes;
    rar_filter30_t   *filter_stack;
    int64_t           filterstart;
    int               lastfilternum;
    int               oldfilterlength[RAR30_MAX_FILTERS];
    int               usagecount[RAR30_MAX_FILTERS];

    /* Output tracking */
    uint8_t *out_buf;
    size_t   out_capacity;
} rar30_ctx_t;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */

static int  rar30_alloc_and_parse_codes(rar30_ctx_t *ctx);
static int  rar30_read_filter_from_input(rar30_ctx_t *ctx);
static int  rar30_read_filter_from_ppmd(rar30_ctx_t *ctx);
static int  rar30_parse_filter(rar30_ctx_t *ctx, const uint8_t *bytes, int length, int flags);
static int  rar30_apply_filters(rar30_ctx_t *ctx, int64_t start, int64_t end, size_t *out_pos);
static void rar30_free_codes(rar30_ctx_t *ctx);

/* ------------------------------------------------------------------ */
/* Huffman code allocation / parsing                                   */
/* ------------------------------------------------------------------ */

static void rar30_free_codes(rar30_ctx_t *ctx)
{
    if(ctx->codes_valid)
    {
        rar_huff_free(&ctx->maincode);
        rar_huff_free(&ctx->offsetcode);
        rar_huff_free(&ctx->lowoffsetcode);
        rar_huff_free(&ctx->lengthcode);
        ctx->codes_valid = 0;
    }
}

static int rar30_alloc_and_parse_codes(rar30_ctx_t *ctx)
{
    rar_bitstream_t *bs = &ctx->bs;

    rar30_free_codes(ctx);

    rar_bs_skip_to_byte_boundary(bs);

    ctx->ppmblock = rar_bs_read_bit(bs);

    if(ctx->ppmblock)
    {
        int flags = (int)rar_bs_read_bits(bs, 7);

        int maxalloc = 0;
        if(flags & 0x20) maxalloc = (int)rar_bs_read_byte(bs);

        if(flags & 0x40) ctx->ppmescape = (int)rar_bs_read_byte(bs);

        if(flags & 0x20)
        {
            int maxorder = (flags & 0x1f) + 1;
            if(maxorder > 16) maxorder = 16 + (maxorder - 16) * 3;

            /* maxorder==1 is end-of-file marker */
            if(maxorder == 1) return -1;

            if(ctx->alloc) FreeSubAllocatorVariantH(ctx->alloc);
            ctx->alloc = CreateSubAllocatorVariantH((maxalloc + 1) << 20);
            if(!ctx->alloc) return -1;

            StartPPMdModelVariantH(&ctx->ppmd, ppmd_read_byte, bs, ctx->alloc, maxorder, false);
            ctx->ppmd_initialized = 1;
        }
        else
        {
            if(!ctx->ppmd_initialized) return -1;
            RestartPPMdVariantHRangeCoder(&ctx->ppmd, ppmd_read_byte, bs, false);
        }

        return 0;
    }

    /* Huffman mode */
    ctx->lastlowoffset       = 0;
    ctx->numlowoffsetrepeats = 0;

    if(!rar_bs_read_bit(bs)) memset(ctx->lengthtable, 0, sizeof(ctx->lengthtable));

    /* Read 20-symbol precode with 4-bit lengths */
    int prelengths[20];
    for(int i = 0; i < 20;)
    {
        int length = (int)rar_bs_read_bits(bs, 4);
        if(length == 15)
        {
            int count = (int)rar_bs_read_bits(bs, 4) + 2;
            if(count == 2)
                prelengths[i++] = 15;
            else
            {
                for(int j = 0; j < count && i < 20; j++) prelengths[i++] = 0;
            }
        }
        else
        {
            prelengths[i++] = length;
        }
    }

    rar_huff_code_t precode;
    if(rar_huff_create_from_lengths(&precode, prelengths, 20, 15) != 0) return -1;

    /* Decode cumulative code lengths */
    for(int i = 0; i < RAR30_TABLE_SIZE;)
    {
        int val = rar_huff_decode(&precode, bs);
        if(val < 0)
        {
            rar_huff_free(&precode);
            return -1;
        }

        if(val < 16)
        {
            ctx->lengthtable[i] = (ctx->lengthtable[i] + val) & 0x0f;
            i++;
        }
        else if(val < 18)
        {
            if(i == 0)
            {
                rar_huff_free(&precode);
                return -1;
            }

            int n;
            if(val == 16)
                n = (int)rar_bs_read_bits(bs, 3) + 3;
            else
                n = (int)rar_bs_read_bits(bs, 7) + 11;

            for(int j = 0; j < n && i < RAR30_TABLE_SIZE; j++)
            {
                ctx->lengthtable[i] = ctx->lengthtable[i - 1];
                i++;
            }
        }
        else /* val == 18 or 19 */
        {
            int n;
            if(val == 18)
                n = (int)rar_bs_read_bits(bs, 3) + 3;
            else
                n = (int)rar_bs_read_bits(bs, 7) + 11;

            for(int j = 0; j < n && i < RAR30_TABLE_SIZE; j++) ctx->lengthtable[i++] = 0;
        }
    }

    rar_huff_free(&precode);

    /* Build the four Huffman codes */
    if(rar_huff_create_from_lengths(&ctx->maincode, &ctx->lengthtable[0], RAR30_NUM_MAIN, 15) != 0) return -1;
    ctx->codes_valid = 1;

    if(rar_huff_create_from_lengths(&ctx->offsetcode, &ctx->lengthtable[RAR30_NUM_MAIN], RAR30_NUM_OFFSET, 15) != 0)
        return -1;

    if(rar_huff_create_from_lengths(&ctx->lowoffsetcode, &ctx->lengthtable[RAR30_NUM_MAIN + RAR30_NUM_OFFSET],
                                    RAR30_NUM_LOWOFFSET, 15) != 0)
        return -1;

    if(rar_huff_create_from_lengths(&ctx->lengthcode,
                                    &ctx->lengthtable[RAR30_NUM_MAIN + RAR30_NUM_OFFSET + RAR30_NUM_LOWOFFSET],
                                    RAR30_NUM_LENGTH, 15) != 0)
        return -1;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Filter: read from Huffman bitstream                                 */
/* ------------------------------------------------------------------ */

static int rar30_read_filter_from_input(rar30_ctx_t *ctx)
{
    rar_bitstream_t *bs = &ctx->bs;

    int flags = (int)rar_bs_read_bits(bs, 8);

    int length = (flags & 7) + 1;
    if(length == 7)
        length = (int)rar_bs_read_bits(bs, 8) + 7;
    else if(length == 8)
        length = (int)rar_bs_read_bits(bs, 16);

    if(length <= 0 || length > 0x10000) return -1;

    uint8_t *code = (uint8_t *)malloc((size_t)length);
    if(!code) return -1;

    for(int i = 0; i < length; i++) code[i] = (uint8_t)rar_bs_read_bits(bs, 8);

    int ret = rar30_parse_filter(ctx, code, length, flags);
    free(code);
    return ret;
}

/* ------------------------------------------------------------------ */
/* Filter: read from PPMd byte stream                                  */
/* ------------------------------------------------------------------ */

static int rar30_read_filter_from_ppmd(rar30_ctx_t *ctx)
{
    int flags = NextPPMdVariantHByte(&ctx->ppmd);
    if(flags < 0) return -1;

    int length = (flags & 7) + 1;
    if(length == 7)
    {
        int b = NextPPMdVariantHByte(&ctx->ppmd);
        if(b < 0) return -1;
        length = b + 7;
    }
    else if(length == 8)
    {
        int b1 = NextPPMdVariantHByte(&ctx->ppmd);
        int b0 = NextPPMdVariantHByte(&ctx->ppmd);
        if(b1 < 0 || b0 < 0) return -1;
        length = (b1 << 8) | b0;
    }

    if(length <= 0 || length > 0x10000) return -1;

    uint8_t *code = (uint8_t *)malloc((size_t)length);
    if(!code) return -1;

    for(int i = 0; i < length; i++)
    {
        int b = NextPPMdVariantHByte(&ctx->ppmd);
        if(b < 0)
        {
            free(code);
            return -1;
        }
        code[i] = (uint8_t)b;
    }

    int ret = rar30_parse_filter(ctx, code, length, flags);
    free(code);
    return ret;
}

/* ------------------------------------------------------------------ */
/* Filter: parse bytecode and create filter invocation                 */
/* ------------------------------------------------------------------ */

static int rar30_parse_filter(rar30_ctx_t *ctx, const uint8_t *bytes, int length, int flags)
{
    /* Create a bitstream-like reader on the filter data.
     * The filter data uses its own variable-length encoding, not the
     * main bitstream. We use a simple byte-position reader. */

    /* Use byte-level reader via filter_read_vm_number for VM numbers,
     * and rar_bitstream_t for bit-level access within the filter data. */
    rar_bitstream_t fbs;
    rar_bs_init(&fbs, bytes, (size_t)length);

    int num;
    int isnew = 0;

    /* Read filter number */
    if(flags & 0x80)
    {
        num = (int)rar_vm_read_number(&fbs) - 1;

        if(num == -1)
        {
            /* Clear all filters */
            num = 0;
            for(int i = 0; i < ctx->num_filtercodes; i++)
            {
                if(ctx->filtercode[i]) rar_vm_program_free(ctx->filtercode[i]);
                ctx->filtercode[i] = NULL;
            }
            ctx->num_filtercodes = 0;
            rar_filter30_free_chain(ctx->filter_stack);
            ctx->filter_stack = NULL;
        }

        if(num > ctx->num_filtercodes || num < 0 || num >= RAR30_MAX_FILTERS) return -1;

        if(num == ctx->num_filtercodes)
        {
            isnew                     = 1;
            ctx->oldfilterlength[num] = 0;
            ctx->usagecount[num]      = -1;
        }

        ctx->lastfilternum = num;
    }
    else
    {
        num = ctx->lastfilternum;
    }

    ctx->usagecount[num]++;

    /* Read filter range */
    int64_t blockstartpos = (int64_t)rar_vm_read_number(&fbs) + ctx->lzss.position;
    if(flags & 0x40) blockstartpos += 258;

    uint32_t blocklength;
    if(flags & 0x20)
    {
        blocklength               = rar_vm_read_number(&fbs);
        ctx->oldfilterlength[num] = (int)blocklength;
    }
    else
    {
        blocklength = (uint32_t)ctx->oldfilterlength[num];
    }

    uint32_t registers[8];
    memset(registers, 0, sizeof(registers));
    registers[3] = RAR_VM_SYS_GLOBAL;
    registers[4] = blocklength;
    registers[5] = (uint32_t)ctx->usagecount[num];
    registers[7] = RAR_VM_MEM_SIZE;

    /* Read register override values */
    if(flags & 0x10)
    {
        int mask = (int)rar_bs_read_bits(&fbs, 7);
        for(int i = 0; i < 7; i++)
        {
            if(mask & (1 << i)) registers[i] = rar_vm_read_number(&fbs);
        }
    }

    /* Read bytecode or look up old version */
    rar_vm_program_t *prog;
    if(isnew)
    {
        int codelen = (int)rar_vm_read_number(&fbs);
        if(codelen <= 0 || codelen > 0x10000) return -1;

        uint8_t *bytecode = (uint8_t *)malloc((size_t)codelen);
        if(!bytecode) return -1;

        for(int i = 0; i < codelen; i++) bytecode[i] = (uint8_t)rar_bs_read_bits(&fbs, 8);

        prog = rar_vm_program_create(bytecode, codelen);
        free(bytecode);
        if(!prog) return -1;

        ctx->filtercode[num] = prog;
        ctx->num_filtercodes = num + 1;
    }
    else
    {
        if(num >= ctx->num_filtercodes || !ctx->filtercode[num]) return -1;
        prog = ctx->filtercode[num];
    }

    /* Read optional global data */
    uint8_t *global_data     = NULL;
    int      global_data_len = 0;
    if(flags & 0x08)
    {
        global_data_len = (int)rar_vm_read_number(&fbs);
        if(global_data_len < 0 || global_data_len > (int)RAR_VM_USR_GLOBAL_SZ) return -1;

        int total_len = global_data_len + RAR_VM_SYS_GLOBAL_SZ;
        global_data   = (uint8_t *)calloc(1, (size_t)total_len);
        if(!global_data) return -1;

        for(int i = 0; i < global_data_len; i++)
            global_data[i + RAR_VM_SYS_GLOBAL_SZ] = (uint8_t)rar_bs_read_bits(&fbs, 8);

        global_data_len = total_len;
    }

    /* Create invocation */
    rar_vm_invocation_t *inv = rar_vm_invocation_create(prog, global_data, global_data_len, registers);
    if(global_data) free(global_data);
    if(!inv) return -1;

    /* Write system globals */
    for(int i = 0; i < 7; i++) rar_vm_invocation_set_global32(inv, i * 4, registers[i]);
    rar_vm_invocation_set_global32(inv, 0x1c, blocklength);
    rar_vm_invocation_set_global32(inv, 0x20, 0);
    rar_vm_invocation_set_global32(inv, 0x2c, (uint32_t)ctx->usagecount[num]);

    /* Create filter and add to stack */
    rar_filter30_t *filter = rar_filter30_create(inv, blockstartpos, (int)blocklength);
    if(!filter)
    {
        rar_vm_invocation_free(inv);
        return -1;
    }

    /* Append to end of filter stack (linked list) */
    if(!ctx->filter_stack)
    {
        ctx->filter_stack = filter;
        ctx->filterstart  = blockstartpos;
    }
    else
    {
        rar_filter30_t *tail = ctx->filter_stack;
        while(tail->next) tail = tail->next;
        tail->next = filter;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Filter application at output time                                   */
/* ------------------------------------------------------------------ */

static int rar30_apply_filters(rar30_ctx_t *ctx, int64_t start, int64_t end, size_t *out_pos)
{
    /* Copy unfiltered data before filter start */
    if(start < ctx->filterstart)
    {
        int64_t copy_end = (ctx->filterstart < end) ? ctx->filterstart : end;
        size_t  len      = (size_t)(copy_end - start);
        if(*out_pos + len > ctx->out_capacity) len = ctx->out_capacity - *out_pos;
        rar_lzss_copy_bytes(&ctx->lzss, ctx->out_buf + *out_pos, start, len);
        *out_pos += len;
    }

    /* Process filters */
    while(ctx->filter_stack && ctx->filter_stack->block_start <= end)
    {
        rar_filter30_t *f       = ctx->filter_stack;
        int64_t         fstart  = f->block_start;
        int             flength = f->block_length;

        /* Copy data to VM memory */
        uint8_t *memory = ctx->vm.memory;
        rar_lzss_copy_bytes(&ctx->lzss, memory, fstart, (size_t)flength);

        /* Execute filter */
        rar_filter30_execute(f, &ctx->vm, fstart);

        uint32_t filtered_addr = f->filtered_addr;
        uint32_t filtered_len  = f->filtered_len;

        /* Remove from stack */
        ctx->filter_stack = f->next;
        f->next           = NULL;
        rar_filter30_free(f);

        /* Execute any chained filters at the same position */
        while(ctx->filter_stack)
        {
            rar_filter30_t *nf = ctx->filter_stack;
            if(nf->block_start != fstart) break;
            if(nf->block_length != (int)filtered_len) break;

            memmove(&memory[0], &memory[filtered_addr], filtered_len);

            rar_filter30_execute(nf, &ctx->vm, fstart);
            filtered_addr = nf->filtered_addr;
            filtered_len  = nf->filtered_len;

            ctx->filter_stack = nf->next;
            nf->next          = NULL;
            rar_filter30_free(nf);
        }

        /* Copy filtered output */
        if(filtered_len > 0)
        {
            size_t copy = filtered_len;
            if(*out_pos + copy > ctx->out_capacity) copy = ctx->out_capacity - *out_pos;
            memcpy(ctx->out_buf + *out_pos, &memory[filtered_addr], copy);
            *out_pos += copy;
        }

        /* Copy any gap between filter end and next filter/end */
        int64_t fend          = fstart + flength;
        int64_t next_boundary = end;
        if(ctx->filter_stack && ctx->filter_stack->block_start < next_boundary)
            next_boundary = ctx->filter_stack->block_start;

        if(fend < next_boundary)
        {
            size_t gap = (size_t)(next_boundary - fend);
            if(*out_pos + gap > ctx->out_capacity) gap = ctx->out_capacity - *out_pos;
            rar_lzss_copy_bytes(&ctx->lzss, ctx->out_buf + *out_pos, fend, gap);
            *out_pos += gap;
        }

        /* Update filterstart */
        if(ctx->filter_stack)
            ctx->filterstart = ctx->filter_stack->block_start;
        else
            ctx->filterstart = INT64_MAX;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Main decompression loop                                             */
/* ------------------------------------------------------------------ */

int rar30_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    if(!in_buf || !out_buf || !out_len || *out_len == 0) return -1;

    rar30_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    rar_bs_init(&ctx.bs, in_buf, in_len);

    if(rar_lzss_init(&ctx.lzss, RAR30_WINDOW_SIZE) != 0) return -1;

    ctx.ppmescape     = 2;
    ctx.filterstart   = INT64_MAX;
    ctx.lastfilternum = 0;
    ctx.out_buf       = out_buf;
    ctx.out_capacity  = *out_len;

    rar_vm_init(&ctx.vm);

    size_t target_len = *out_len;
    *out_len          = 0;

    int ret = -1;

    /* Parse initial block header */
    if(rar30_alloc_and_parse_codes(&ctx) != 0) goto cleanup;

    /* Main decode loop */
    while((size_t)ctx.lzss.position < target_len)
    {
        /* Check if we've hit a filter boundary */
        if(ctx.lzss.position >= ctx.filterstart)
        {
            /* We need to stop and apply filters — but in single-buffer mode
             * we continue decoding and apply filters at the end */
        }

        if(ctx.ppmblock)
        {
            int byte = NextPPMdVariantHByte(&ctx.ppmd);
            if(byte < 0) goto cleanup;

            if(byte != ctx.ppmescape) { rar_lzss_emit_literal(&ctx.lzss, (uint8_t)byte); }
            else
            {
                int code = NextPPMdVariantHByte(&ctx.ppmd);
                if(code < 0) goto cleanup;

                switch(code)
                {
                    case 0:
                        if(rar30_alloc_and_parse_codes(&ctx) != 0) goto cleanup;
                        break;

                    case 2:
                        /* End of data */
                        goto done;

                    case 3:
                        if(rar30_read_filter_from_ppmd(&ctx) != 0) goto cleanup;
                        break;

                    case 4:
                    {
                        int b2 = NextPPMdVariantHByte(&ctx.ppmd);
                        int b1 = NextPPMdVariantHByte(&ctx.ppmd);
                        int b0 = NextPPMdVariantHByte(&ctx.ppmd);
                        int bl = NextPPMdVariantHByte(&ctx.ppmd);
                        if(b2 < 0 || b1 < 0 || b0 < 0 || bl < 0) goto cleanup;

                        int offs = (b2 << 16) | (b1 << 8) | b0;
                        int len  = bl;
                        rar_lzss_emit_match(&ctx.lzss, offs + 2, len + 32);
                        break;
                    }

                    case 5:
                    {
                        int bl = NextPPMdVariantHByte(&ctx.ppmd);
                        if(bl < 0) goto cleanup;
                        rar_lzss_emit_match(&ctx.lzss, 1, bl + 4);
                        break;
                    }

                    default:
                        rar_lzss_emit_literal(&ctx.lzss, (uint8_t)byte);
                        break;
                }
            }
        }
        else
        {
            int symbol = rar_huff_decode(&ctx.maincode, &ctx.bs);
            if(symbol < 0) goto cleanup;

            if(symbol < 256)
            {
                rar_lzss_emit_literal(&ctx.lzss, (uint8_t)symbol);
                continue;
            }

            if(symbol == 256)
            {
                int newfile = !rar_bs_read_bit(&ctx.bs);
                if(newfile)
                {
                    /* End of data for this file. Check if new table requested. */
                    /* int newtable = */ rar_bs_read_bit(&ctx.bs);
                    goto done;
                }
                else
                {
                    if(rar30_alloc_and_parse_codes(&ctx) != 0) goto cleanup;
                    continue;
                }
            }

            if(symbol == 257)
            {
                if(rar30_read_filter_from_input(&ctx) != 0) goto cleanup;
                continue;
            }

            int offs, len;

            if(symbol == 258)
            {
                /* Repeat last match */
                if(ctx.lastlength == 0) continue;
                offs = ctx.lastoffset;
                len  = ctx.lastlength;
            }
            else if(symbol <= 262)
            {
                /* Old offset reuse */
                int offsindex = symbol - 259;
                offs          = ctx.oldoffset[offsindex];

                int lensymbol = rar_huff_decode(&ctx.lengthcode, &ctx.bs);
                if(lensymbol < 0) goto cleanup;

                len = lengthbases[lensymbol] + 2;
                if(lengthbits[lensymbol] > 0) len += (int)rar_bs_read_bits(&ctx.bs, lengthbits[lensymbol]);

                for(int i = offsindex; i > 0; i--) ctx.oldoffset[i] = ctx.oldoffset[i - 1];
                ctx.oldoffset[0] = offs;
            }
            else if(symbol <= 270)
            {
                /* Short match */
                int idx = symbol - 263;
                offs    = shortbases[idx] + 1;
                if(shortbits[idx] > 0) offs += (int)rar_bs_read_bits(&ctx.bs, shortbits[idx]);

                len = 2;

                for(int i = 3; i > 0; i--) ctx.oldoffset[i] = ctx.oldoffset[i - 1];
                ctx.oldoffset[0] = offs;
            }
            else /* symbol >= 271 */
            {
                /* Full match */
                int sym_idx = symbol - 271;
                if(sym_idx >= 28) goto cleanup;

                len = lengthbases[sym_idx] + 3;
                if(lengthbits[sym_idx] > 0) len += (int)rar_bs_read_bits(&ctx.bs, lengthbits[sym_idx]);

                int offssymbol = rar_huff_decode(&ctx.offsetcode, &ctx.bs);
                if(offssymbol < 0) goto cleanup;

                offs = offsetbases[offssymbol] + 1;
                if(offsetbits[offssymbol] > 0)
                {
                    if(offssymbol > 9)
                    {
                        if(offsetbits[offssymbol] > 4)
                            offs += (int)rar_bs_read_bits(&ctx.bs, offsetbits[offssymbol] - 4) << 4;

                        if(ctx.numlowoffsetrepeats > 0)
                        {
                            ctx.numlowoffsetrepeats--;
                            offs += ctx.lastlowoffset;
                        }
                        else
                        {
                            int lowoffsetsymbol = rar_huff_decode(&ctx.lowoffsetcode, &ctx.bs);
                            if(lowoffsetsymbol < 0) goto cleanup;

                            if(lowoffsetsymbol == 16)
                            {
                                ctx.numlowoffsetrepeats = 15;
                                offs += ctx.lastlowoffset;
                            }
                            else
                            {
                                offs += lowoffsetsymbol;
                                ctx.lastlowoffset = lowoffsetsymbol;
                            }
                        }
                    }
                    else
                    {
                        offs += (int)rar_bs_read_bits(&ctx.bs, offsetbits[offssymbol]);
                    }
                }

                if(offs >= 0x40000) len++;
                if(offs >= 0x2000) len++;

                for(int i = 3; i > 0; i--) ctx.oldoffset[i] = ctx.oldoffset[i - 1];
                ctx.oldoffset[0] = offs;
            }

            ctx.lastoffset = offs;
            ctx.lastlength = len;

            rar_lzss_emit_match(&ctx.lzss, offs, len);
        }
    }

done:
    /* Produce output: apply filters and copy from LZSS window */
    {
        int64_t total = ctx.lzss.position;
        if((size_t)total > target_len) total = (int64_t)target_len;

        if(ctx.filter_stack)
        {
            /* Walk through all data, applying filters where they exist */
            int64_t pos     = 0;
            size_t  out_pos = 0;

            while(pos < total && out_pos < ctx.out_capacity)
            {
                if(ctx.filter_stack && pos == ctx.filter_stack->block_start)
                {
                    rar30_apply_filters(&ctx, pos, total, &out_pos);
                    /* Advance pos past all applied filters */
                    if(ctx.filter_stack)
                        pos = ctx.filter_stack->block_start;
                    else
                        pos = total;
                }
                else
                {
                    /* Copy unfiltered data up to next filter or end */
                    int64_t next = total;
                    if(ctx.filter_stack && ctx.filter_stack->block_start < next) next = ctx.filter_stack->block_start;

                    size_t len = (size_t)(next - pos);
                    if(out_pos + len > ctx.out_capacity) len = ctx.out_capacity - out_pos;
                    rar_lzss_copy_bytes(&ctx.lzss, ctx.out_buf + out_pos, pos, len);
                    out_pos += len;
                    pos = next;
                }
            }

            *out_len = out_pos;
        }
        else
        {
            /* No filters: straight copy from LZSS window */
            rar_lzss_copy_bytes(&ctx.lzss, out_buf, 0, (size_t)total);
            *out_len = (size_t)total;
        }
    }

    ret = 0;

cleanup:
    rar30_free_codes(&ctx);
    rar_lzss_cleanup(&ctx.lzss);
    if(ctx.alloc) FreeSubAllocatorVariantH(ctx.alloc);
    rar_filter30_free_chain(ctx.filter_stack);
    for(int i = 0; i < ctx.num_filtercodes; i++)
    {
        if(ctx.filtercode[i]) rar_vm_program_free(ctx.filtercode[i]);
    }
    return ret;
}
