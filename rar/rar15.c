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
 * RAR 1.5 (UNP_VER=15) decompressor.
 *
 * 64 KB LZSS window, 13 fixed Huffman tables, frequency-based table
 * selection via running averages, byte-reordering lookup tables.
 */

#include <string.h>

#include "bitstream.h"
#include "huffman.h"
#include "rar.h"

/* ------------------------------------------------------------------ */
/* Context                                                             */
/* ------------------------------------------------------------------ */

typedef struct
{
    rar_bitstream_t bs;

    rar_huff_code_t huffmancode[5];    /* literal/offset codes, 257 syms  */
    rar_huff_code_t shortmatchcode[4]; /* short-match selector codes      */
    rar_huff_code_t lengthcode[2];     /* length codes, 256 syms          */

    int      storedblock;
    unsigned flags, flagbits;
    unsigned literalweight, matchweight;
    unsigned numrepeatedliterals, numrepeatedlastmatches;
    unsigned runningaverageliteral, runningaverageselector;
    unsigned runningaveragelength, runningaverageoffset;
    unsigned runningaveragebelowmaximum;
    unsigned maximumoffset;
    int      bugfixflag;

    int lastoffset, lastlength;
    int oldoffset[4];
    int oldoffsetindex;

    int flagtable[256], flagreverse[256];
    int literaltable[256], literalreverse[256];
    int offsettable[256], offsetreverse[256];
    int shortoffsettable[256];

    uint8_t *out_buf;
    size_t   out_len;
    size_t   out_pos;
    int      error;
} rar15_ctx_t;

/* ------------------------------------------------------------------ */
/* Lookup-byte / table helpers                                         */
/* ------------------------------------------------------------------ */

static void reset_table(int *table, int *reverse)
{
    for(int i = 0; i < 8; i++)
        for(int j = 0; j < 32; j++) table[i * 32 + j] = (table[i * 32 + j] & ~0xff) | (7 - i);

    memset(reverse, 0, sizeof(int) * 256);
    for(int i = 0; i < 7; i++) reverse[i] = (7 - i) * 32;
}

static int lookup_byte(int *table, int *reverse, int limit, int index)
{
    int val      = table[index];
    int newindex = reverse[val & 0xff]++;

    if((val & 0xff) >= limit)
    {
        reset_table(table, reverse);
        val      = table[index];
        newindex = reverse[val & 0xff]++;
    }

    table[index]    = table[newindex];
    table[newindex] = val + 1;

    return val >> 8;
}

/* ------------------------------------------------------------------ */
/* Output helpers                                                      */
/* ------------------------------------------------------------------ */

static inline void emit_byte(rar15_ctx_t *ctx, uint8_t byte)
{
    if(ctx->out_pos < ctx->out_len) ctx->out_buf[ctx->out_pos] = byte;
    ctx->out_pos++;
}

static void emit_match(rar15_ctx_t *ctx, int offset, int length)
{
    for(int i = 0; i < length; i++)
    {
        uint8_t b = 0;
        if(offset > 0 && ctx->out_pos >= (size_t)offset) b = ctx->out_buf[ctx->out_pos - offset];
        emit_byte(ctx, b);
    }
}

/* ------------------------------------------------------------------ */
/* Flag-bit reader                                                     */
/* ------------------------------------------------------------------ */

static int get_flag_bit(rar15_ctx_t *ctx)
{
    if(ctx->flagbits == 0)
    {
        int index = rar_huff_decode(&ctx->huffmancode[2], &ctx->bs);
        if(index < 0 || index == 256)
        {
            ctx->error = 1;
            return 0;
        }
        ctx->flags    = (unsigned)lookup_byte(ctx->flagtable, ctx->flagreverse, 0xff, index);
        ctx->flagbits = 8;
    }
    ctx->flagbits--;
    return (int)((ctx->flags >> ctx->flagbits) & 1);
}

/* ------------------------------------------------------------------ */
/* Symbol dispatch: literal                                            */
/* ------------------------------------------------------------------ */

static void do_emit_literal(rar15_ctx_t *ctx)
{
    int index;

    if(ctx->runningaverageliteral < 0xe00)
        index = rar_huff_decode(&ctx->huffmancode[0], &ctx->bs);
    else if(ctx->runningaverageliteral < 0x3600)
        index = rar_huff_decode(&ctx->huffmancode[1], &ctx->bs);
    else if(ctx->runningaverageliteral < 0x5e00)
        index = rar_huff_decode(&ctx->huffmancode[2], &ctx->bs);
    else if(ctx->runningaverageliteral < 0x7600)
        index = rar_huff_decode(&ctx->huffmancode[3], &ctx->bs);
    else
        index = rar_huff_decode(&ctx->huffmancode[4], &ctx->bs);

    if(index < 0)
    {
        ctx->error = 1;
        return;
    }

    if(ctx->storedblock)
    {
        if(index == 0)
        {
            if(rar_bs_read_bit(&ctx->bs))
            {
                /* End stored block */
                ctx->storedblock         = 0;
                ctx->numrepeatedliterals = 0;
                return;
            }
            else
            {
                /* Embedded match inside stored block */
                int length   = rar_bs_read_bit(&ctx->bs) ? 4 : 3;
                int offset_h = rar_huff_decode(&ctx->huffmancode[2], &ctx->bs);
                if(offset_h < 0)
                {
                    ctx->error = 1;
                    return;
                }
                int offset = (offset_h << 5) | (int)rar_bs_read_bits(&ctx->bs, 5);
                emit_match(ctx, offset, length);
                return;
            }
        }
        else
        {
            index--;
        }
    }
    else
    {
        index &= 0xff;
        if(ctx->numrepeatedliterals++ >= 16 && ctx->flagbits == 0) ctx->storedblock = 1;
    }

    ctx->runningaverageliteral += (unsigned)index;
    ctx->runningaverageliteral -= ctx->runningaverageliteral >> 8;

    ctx->literalweight += 16;
    if(ctx->literalweight > 0xff)
    {
        ctx->literalweight = 0x90;
        ctx->matchweight >>= 1;
    }

    uint8_t byte = (uint8_t)lookup_byte(ctx->literaltable, ctx->literalreverse, 0xa1, index);
    emit_byte(ctx, byte);
}

/* ------------------------------------------------------------------ */
/* Symbol dispatch: long match                                         */
/* ------------------------------------------------------------------ */

static void do_emit_long_match(rar15_ctx_t *ctx)
{
    ctx->numrepeatedliterals = 0;

    ctx->matchweight += 16;
    if(ctx->matchweight > 0xff)
    {
        ctx->matchweight = 0x90;
        ctx->literalweight >>= 1;
    }

    /* --- raw length --- */
    int rawlength;
    if(ctx->runningaveragelength >= 122)
        rawlength = rar_huff_decode(&ctx->lengthcode[1], &ctx->bs);
    else if(ctx->runningaveragelength >= 64)
        rawlength = rar_huff_decode(&ctx->lengthcode[0], &ctx->bs);
    else
    {
        rawlength = 0;
        while(rawlength < 8 && rar_bs_read_bit(&ctx->bs) == 0) rawlength++;
        if(rawlength == 8) rawlength = (int)rar_bs_read_bits(&ctx->bs, 8);
    }
    if(rawlength < 0)
    {
        ctx->error = 1;
        return;
    }

    /* --- offset --- */
    int offsetindex;
    if(ctx->runningaverageoffset < 0x700)
        offsetindex = rar_huff_decode(&ctx->huffmancode[0], &ctx->bs);
    else if(ctx->runningaverageoffset < 0x2900)
        offsetindex = rar_huff_decode(&ctx->huffmancode[1], &ctx->bs);
    else
        offsetindex = rar_huff_decode(&ctx->huffmancode[2], &ctx->bs);

    if(offsetindex < 0)
    {
        ctx->error = 1;
        return;
    }
    if(offsetindex == 0x100)
    {
        ctx->error = 1;
        return;
    }

    int offset = lookup_byte(ctx->offsettable, ctx->offsetreverse, 0xff, offsetindex) << 7;
    offset |= (int)rar_bs_read_bits(&ctx->bs, 7);

    int length = rawlength + 3;
    if(offset >= (int)ctx->maximumoffset) length++;
    if(offset <= 256) length += 8;

    unsigned old_max = ctx->maximumoffset;

    if(ctx->runningaveragebelowmaximum > 0xb0 ||
       (ctx->runningaverageliteral >= 0x2a00 && ctx->runningaveragelength < 0x40))
        ctx->maximumoffset = 0x7f00;
    else
        ctx->maximumoffset = 0x2001;

    ctx->runningaveragelength += (unsigned)rawlength;
    ctx->runningaveragelength -= ctx->runningaveragelength >> 5;

    ctx->runningaverageoffset += (unsigned)offsetindex;
    ctx->runningaverageoffset -= ctx->runningaverageoffset >> 8;

    if(rawlength == 0 && offset <= (int)old_max)
    {
        ctx->runningaveragebelowmaximum++;
        ctx->runningaveragebelowmaximum -= ctx->runningaveragebelowmaximum >> 8;
    }
    else if(rawlength != 1 && rawlength != 4)
    {
        if(ctx->runningaveragebelowmaximum > 0) ctx->runningaveragebelowmaximum--;
    }

    ctx->lastoffset = ctx->oldoffset[ctx->oldoffsetindex++ & 3] = offset;
    ctx->lastlength                                             = length;

    emit_match(ctx, ctx->lastoffset, ctx->lastlength);
}

/* ------------------------------------------------------------------ */
/* Symbol dispatch: short match                                        */
/* ------------------------------------------------------------------ */

static void do_emit_short_match(rar15_ctx_t *ctx)
{
    ctx->numrepeatedliterals = 0;

    if(ctx->numrepeatedlastmatches == 2)
    {
        if(rar_bs_read_bit(&ctx->bs))
        {
            emit_match(ctx, ctx->lastoffset, ctx->lastlength);
            return;
        }
        else
        {
            ctx->numrepeatedlastmatches = 0;
        }
    }

    int selector;
    if(ctx->runningaverageselector < 37)
    {
        if(ctx->bugfixflag)
            selector = rar_huff_decode(&ctx->shortmatchcode[0], &ctx->bs);
        else
            selector = rar_huff_decode(&ctx->shortmatchcode[1], &ctx->bs);
    }
    else
    {
        if(ctx->bugfixflag)
            selector = rar_huff_decode(&ctx->shortmatchcode[2], &ctx->bs);
        else
            selector = rar_huff_decode(&ctx->shortmatchcode[3], &ctx->bs);
    }
    if(selector < 0)
    {
        ctx->error = 1;
        return;
    }

    if(selector < 9)
    {
        /* New short match */
        ctx->numrepeatedlastmatches = 0;

        ctx->runningaverageselector += (unsigned)selector;
        ctx->runningaverageselector -= ctx->runningaverageselector >> 4;

        int offsetindex = rar_huff_decode(&ctx->huffmancode[2], &ctx->bs);
        if(offsetindex < 0)
        {
            ctx->error = 1;
            return;
        }
        offsetindex &= 0xff;

        int offset = ctx->shortoffsettable[offsetindex];
        if(offsetindex != 0)
        {
            ctx->shortoffsettable[offsetindex]     = ctx->shortoffsettable[offsetindex - 1];
            ctx->shortoffsettable[offsetindex - 1] = offset;
        }
        offset++;

        int length = selector + 2;

        ctx->lastoffset = ctx->oldoffset[ctx->oldoffsetindex++ & 3] = offset;
        ctx->lastlength                                             = length;

        emit_match(ctx, offset, length);
    }
    else if(selector == 9)
    {
        /* Repeat last match */
        ctx->numrepeatedlastmatches++;
        emit_match(ctx, ctx->lastoffset, ctx->lastlength);
    }
    else if(selector < 14)
    {
        /* Old offset ring match */
        ctx->numrepeatedlastmatches = 0;

        int offset = ctx->oldoffset[(ctx->oldoffsetindex - (selector - 9)) & 3];

        int length_val = rar_huff_decode(&ctx->lengthcode[0], &ctx->bs);
        if(length_val < 0)
        {
            ctx->error = 1;
            return;
        }
        int length = length_val + 2;

        if(length == 0x101 && selector == 10)
        {
            ctx->bugfixflag = !ctx->bugfixflag;
            return;
        }

        if(offset > 256) length++;
        if(offset >= (int)ctx->maximumoffset) length++;

        ctx->lastoffset = ctx->oldoffset[ctx->oldoffsetindex++ & 3] = offset;
        ctx->lastlength                                             = length;

        emit_match(ctx, offset, length);
    }
    else /* selector == 14 */
    {
        /* Long-distance match */
        ctx->numrepeatedlastmatches = 0;

        int length_val = rar_huff_decode(&ctx->lengthcode[1], &ctx->bs);
        if(length_val < 0)
        {
            ctx->error = 1;
            return;
        }
        int length = length_val + 5;

        int offset = (int)rar_bs_read_bits(&ctx->bs, 15) + 0x8000;

        ctx->lastoffset = offset;
        ctx->lastlength = length;

        emit_match(ctx, offset, length);
    }
}

/* ------------------------------------------------------------------ */
/* Table construction helpers                                          */
/* ------------------------------------------------------------------ */

/*
 * Descriptor: (count, code_length) pairs terminated by {0, 0}.
 * Builds a code-length array and delegates to rar_huff_create_from_lengths.
 */
typedef struct
{
    int count;
    int length;
} tbl_desc_t;

static int build_from_desc(rar_huff_code_t *code, const tbl_desc_t *desc, int num_symbols, int max_length)
{
    int lengths[257];
    int pos = 0;

    for(int i = 0; desc[i].count > 0; i++)
        for(int j = 0; j < desc[i].count && pos < num_symbols; j++) lengths[pos++] = desc[i].length;

    while(pos < num_symbols) lengths[pos++] = 0;

    return rar_huff_create_from_lengths(code, lengths, num_symbols, max_length);
}

/*
 * Explicit code entry: (symbol, code_value MSB-first, code_length).
 * Terminated by symbol == -1.
 */
typedef struct
{
    int      symbol;
    uint32_t code;
    int      len;
} code_entry_t;

static int build_from_codes(rar_huff_code_t *code, const code_entry_t *entries, int max_length)
{
    rar_huff_init_empty(code, max_length);

    for(int i = 0; entries[i].symbol >= 0; i++)
    {
        uint32_t masked = entries[i].code & ((1u << entries[i].len) - 1);
        if(rar_huff_add_code(code, masked, entries[i].len, entries[i].symbol) < 0) return -1;
    }

    rar_huff_build_table(code);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Static Huffman table descriptors                                    */
/* ------------------------------------------------------------------ */

/* lengthcode1: 256 symbols, max length 12 */
static const tbl_desc_t lengthcode1_desc[] = {
    {  2,  2},
    {  1,  3},
    {  2,  4},
    {  2,  5},
    {  4,  6},
    {  5,  7},
    {  4,  8},
    {  4,  9},
    {  8, 10},
    {224, 12},
    {  0,  0}
};

/* lengthcode2: 256 symbols, max length 12 */
static const tbl_desc_t lengthcode2_desc[] = {
    {  5,  3},
    {  2,  4},
    {  2,  5},
    {  4,  6},
    {  5,  7},
    {  4,  8},
    {  4,  9},
    {  8, 10},
    {  2, 11},
    {220, 12},
    {  0,  0}
};

/* huffmancode0: 257 symbols, max length 12 */
static const tbl_desc_t huffmancode0_desc[] = {
    {  8,  4},
    {  8,  5},
    {  8,  6},
    {  9,  7},
    {224, 12},
    {  0,  0}
};

/* huffmancode1: 257 symbols, max length 12 */
static const tbl_desc_t huffmancode1_desc[] = {
    {  4,  5},
    { 40,  6},
    { 16,  7},
    { 16,  8},
    {  4,  9},
    { 47, 11},
    {130, 12},
    {  0,  0}
};

/* huffmancode2: 257 symbols, max length 10 */
static const tbl_desc_t huffmancode2_desc[] = {
    {  2,  5},
    {  5,  6},
    { 46,  7},
    { 64,  8},
    {116,  9},
    { 24, 10},
    {  0,  0}
};

/* huffmancode3: 257 symbols, max length 10 */
static const tbl_desc_t huffmancode3_desc[] = {
    {  2,  6},
    { 14,  7},
    {202,  8},
    { 33,  9},
    {  6, 10},
    {  0,  0}
};

/* huffmancode4: 257 symbols, max length 9 */
static const tbl_desc_t huffmancode4_desc[] = {
    {255, 8},
    {  2, 9},
    {  0, 0}
};

/* ------------------------------------------------------------------ */
/* Short-match code entries (explicit code/length/symbol triples)       */
/* ------------------------------------------------------------------ */

static const code_entry_t shortmatchcode0_entries[] = {
    { 0, 0x00, 1},
    { 1, 0x0a, 4},
    { 2, 0x0d, 4},
    { 3, 0x0e, 4},
    { 4, 0x1e, 5},
    { 5, 0x3e, 6},
    { 6, 0x7e, 7},
    { 7, 0xfe, 8},
    { 8, 0xff, 8},
    { 9, 0x0c, 4},
    {10, 0x08, 4},
    {11, 0x12, 5},
    {12, 0x26, 6},
    {13, 0x27, 6},
    {14, 0x0b, 4},
    {-1,    0, 0}
};

static const code_entry_t shortmatchcode1_entries[] = {
    { 0, 0x00, 1},
    { 1, 0x05, 3},
    { 2, 0x0d, 4},
    { 3, 0x0e, 4},
    { 4, 0x1e, 5},
    { 5, 0x3e, 6},
    { 6, 0x7e, 7},
    { 7, 0xfe, 8},
    { 8, 0xff, 8},
    { 9, 0x0c, 4},
    {10, 0x08, 4},
    {11, 0x12, 5},
    {12, 0x26, 6},
    {13, 0x27, 6},
    {-1,    0, 0}
};

static const code_entry_t shortmatchcode2_entries[] = {
    { 0, 0x00, 2},
    { 1, 0x02, 3},
    { 2, 0x03, 3},
    { 3, 0x0a, 4},
    { 4, 0x0d, 4},
    { 5, 0x0e, 4},
    { 6, 0x1e, 5},
    { 7, 0x3e, 6},
    { 8, 0x3f, 6},
    { 9, 0x0c, 4},
    {10, 0x08, 4},
    {11, 0x12, 5},
    {12, 0x26, 6},
    {13, 0x27, 6},
    {14, 0x0b, 4},
    {-1,    0, 0}
};

static const code_entry_t shortmatchcode3_entries[] = {
    { 0, 0x00, 2},
    { 1, 0x02, 3},
    { 2, 0x03, 3},
    { 3, 0x05, 3},
    { 4, 0x0d, 4},
    { 5, 0x0e, 4},
    { 6, 0x1e, 5},
    { 7, 0x3e, 6},
    { 8, 0x3f, 6},
    { 9, 0x0c, 4},
    {10, 0x08, 4},
    {11, 0x12, 5},
    {12, 0x26, 6},
    {13, 0x27, 6},
    {-1,    0, 0}
};

/* ------------------------------------------------------------------ */
/* Table initialization                                                */
/* ------------------------------------------------------------------ */

static int init_tables(rar15_ctx_t *ctx)
{
    /* Length codes (256 symbols each) */
    if(build_from_desc(&ctx->lengthcode[0], lengthcode1_desc, 256, 12) < 0) return -1;
    if(build_from_desc(&ctx->lengthcode[1], lengthcode2_desc, 256, 12) < 0) return -1;

    /* Huffman / literal / offset codes (257 symbols each) */
    static const tbl_desc_t *const huff_descs[5] = {huffmancode0_desc, huffmancode1_desc, huffmancode2_desc,
                                                    huffmancode3_desc, huffmancode4_desc};
    static const int               huff_max[5]   = {12, 12, 10, 10, 9};

    for(int i = 0; i < 5; i++)
        if(build_from_desc(&ctx->huffmancode[i], huff_descs[i], 257, huff_max[i]) < 0) return -1;

    /* Short-match selector codes (built from explicit triples) */
    static const code_entry_t *const sm_entries[4] = {shortmatchcode0_entries, shortmatchcode1_entries,
                                                      shortmatchcode2_entries, shortmatchcode3_entries};

    for(int i = 0; i < 4; i++)
        if(build_from_codes(&ctx->shortmatchcode[i], sm_entries[i], 8) < 0) return -1;

    return 0;
}

/* ------------------------------------------------------------------ */
/* State reset                                                         */
/* ------------------------------------------------------------------ */

static void init_state(rar15_ctx_t *ctx)
{
    ctx->storedblock            = 0;
    ctx->flagbits               = 0;
    ctx->flags                  = 0;
    ctx->numrepeatedliterals    = 0;
    ctx->numrepeatedlastmatches = 0;
    ctx->bugfixflag             = 0;

    ctx->runningaverageselector     = 0;
    ctx->runningaverageliteral      = 0x3500;
    ctx->runningaveragelength       = 0;
    ctx->runningaverageoffset       = 0;
    ctx->runningaveragebelowmaximum = 0;

    ctx->maximumoffset = 0x2001;
    ctx->literalweight = 0x80;
    ctx->matchweight   = 0x80;

    for(int i = 0; i < 256; i++)
    {
        ctx->flagtable[i]        = ((-i) & 0xff) << 8;
        ctx->literaltable[i]     = i << 8;
        ctx->offsettable[i]      = i << 8;
        ctx->shortoffsettable[i] = i;
    }

    memset(ctx->flagreverse, 0, sizeof(ctx->flagreverse));
    memset(ctx->literalreverse, 0, sizeof(ctx->literalreverse));
    reset_table(ctx->offsettable, ctx->offsetreverse);

    ctx->lastoffset = 0;
    ctx->lastlength = 0;
    memset(ctx->oldoffset, 0, sizeof(ctx->oldoffset));
    ctx->oldoffsetindex = 0;

    ctx->error = 0;
}

/* ------------------------------------------------------------------ */
/* Cleanup                                                             */
/* ------------------------------------------------------------------ */

static void free_tables(rar15_ctx_t *ctx)
{
    for(int i = 0; i < 5; i++) rar_huff_free(&ctx->huffmancode[i]);
    for(int i = 0; i < 4; i++) rar_huff_free(&ctx->shortmatchcode[i]);
    for(int i = 0; i < 2; i++) rar_huff_free(&ctx->lengthcode[i]);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

AARU_EXPORT int AARU_CALL rar15_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    if(!in_buf || !out_buf || !out_len || *out_len == 0) return -1;

    rar15_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    if(init_tables(&ctx) < 0)
    {
        free_tables(&ctx);
        return -1;
    }

    init_state(&ctx);
    rar_bs_init(&ctx.bs, in_buf, in_len);

    ctx.out_buf = out_buf;
    ctx.out_len = *out_len;
    ctx.out_pos = 0;

    while(ctx.out_pos < ctx.out_len && !ctx.error)
    {
        if(ctx.storedblock) { do_emit_literal(&ctx); }
        else
        {
            int expecting_match = ctx.matchweight > ctx.literalweight;

            int flag1 = get_flag_bit(&ctx);
            if(ctx.error) break;

            if(flag1)
            {
                /* Expected case */
                if(expecting_match)
                    do_emit_long_match(&ctx);
                else
                    do_emit_literal(&ctx);
            }
            else
            {
                int flag2 = get_flag_bit(&ctx);
                if(ctx.error) break;

                if(flag2)
                {
                    /* Unexpected case */
                    if(!expecting_match)
                        do_emit_long_match(&ctx);
                    else
                        do_emit_literal(&ctx);
                }
                else
                {
                    do_emit_short_match(&ctx);
                }
            }
        }
    }

    if(ctx.out_pos < ctx.out_len) *out_len = ctx.out_pos;

    free_tables(&ctx);

    return ctx.error ? -1 : 0;
}
