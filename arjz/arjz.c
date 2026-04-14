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

#include "arjz.h"

#include <stdlib.h>
#include <string.h>

/* Constants */
#define ARJZ_MIN_WINDOW_SIZE 65536

#define LENGTH_CODES  29
#define LITERALS      256
#define REP_BOTH_LIT  256
#define MMBLOCK_LIT   257
#define STR2_LIT      258
#define STR2_LAST_LIT 265
#define STR3_LIT      266
#define STR3_LAST_LIT 301
#define END_BLOCK_LIT 302

#define SPEC_CODES (END_BLOCK_LIT - LITERALS + 1)
#define L_CODES    (LITERALS + SPEC_CODES + LENGTH_CODES)

#define REPDIST_CODES 4
#define D_CODES_LB    6
#define D_CODES       (REPDIST_CODES + 60)

#define BL_CODES 19

/* Huffman table entry types (stored in e field) */
#define LIT_CODE         36
#define EOB_CODE         37
#define REPB_CODE        38
#define MM_CODE          39
#define STR2_CODE        40
#define STR2_LASTCODE    49
#define STR3_CODE        50
#define STR3_LASTCODE    89
#define REPDIST_CODE     90
#define REPDIST_LASTCODE 99
#define TABLE_BASECODE   100
#define NONSENSE_CODE    119

#define BMAX  16
#define N_MAX 512

#define MAX_MMDIST 4

/* Huffman table entry */
typedef struct arjz_huft
{
    uint8_t e; /* extra bits or operation */
    uint8_t b; /* bits in this code */

    union
    {
        uint32_t          n; /* literal, length base, or distance base */
        struct arjz_huft *t; /* pointer to next level of table */
    } v;
} arjz_huft;

/* Decompression context */
typedef struct
{
    /* Input */
    const uint8_t *in_buf;
    size_t         in_len;
    size_t         in_pos;

    /* Bit buffer */
    uint32_t bb;
    unsigned bk;

    /* Output */
    uint8_t *out_buf;
    size_t   out_len;
    size_t   out_pos;

    /* Sliding window */
    uint8_t *window;
    size_t   window_size;
    size_t   window_mask;
    size_t   wp;
    size_t   lastwp;

    /* MM block state */
    size_t   mmstart;
    unsigned mmdist;
    unsigned mmcount;
    unsigned mmtype;
    char     mmvalue[MAX_MMDIST];

    /* E8/E9 state */
    unsigned exediff;
    size_t   filesize;
    size_t   filepos;
    char     hold[5];
    int      bytes_on_hold;

    /* Stream parameters */
    uint32_t md4;

    /* Huffman table memory pool */
    uint8_t *pool;
    size_t   pool_size;
    size_t   pool_used;

    /* Literal/distance code lengths (persistent for DIFF_TREES) */
    unsigned ll[L_CODES + D_CODES];
} arjz_ctx;

/* Bit length code order */
static const unsigned arjz_border[19] = {16, 17, 18, 0, 1, 15, 2, 14, 3, 13, 4, 12, 5, 11, 6, 10, 7, 9, 8};

/* Base distances for literal codes (2-byte strings, 3-byte strings, lengths) */
/* Padded to 256 entries for static block table builder (codes 256-511 need indices 0-255) */
static const uint32_t arjz_cplens[256] = {
    0, 0, /* 2-byte: */ 1, 5, 9, 17, 33, 65, 129, 193,
    /* 3-byte: */ 0, 0, 0, 0, 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
    2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0x8001, 0xC001, 0,
    /* Length bases: */
    4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 44, 52, 60, 68, 84, 100, 116, 132, 164, 196, 228, 260,
    0, 0
    /* remaining entries are zero-initialized */
};

/* Extra bits/codes for literal codes */
/* Padded to 256 entries; extra entries set to NONSENSE_CODE for safety */
static const uint32_t arjz_cplext[256] = {
    REPB_CODE, MM_CODE,
    /* 2-byte strings: */
    STR2_CODE + 2, STR2_CODE + 2, STR2_CODE + 3, STR2_CODE + 4, STR2_CODE + 5, STR2_CODE + 6, STR2_CODE + 6,
    STR2_CODE + 6,
    /* 3-byte strings: */
    REPDIST_CODE, REPDIST_CODE + 1, REPDIST_CODE + 2, REPDIST_CODE + 3, STR3_CODE + 0, STR3_CODE + 0, STR3_CODE + 0,
    STR3_CODE + 0, STR3_CODE + 1, STR3_CODE + 1, STR3_CODE + 2, STR3_CODE + 2, STR3_CODE + 3, STR3_CODE + 3,
    STR3_CODE + 4, STR3_CODE + 4, STR3_CODE + 5, STR3_CODE + 5, STR3_CODE + 6, STR3_CODE + 6, STR3_CODE + 7,
    STR3_CODE + 7, STR3_CODE + 8, STR3_CODE + 8, STR3_CODE + 9, STR3_CODE + 9, STR3_CODE + 10, STR3_CODE + 10,
    STR3_CODE + 11, STR3_CODE + 11, STR3_CODE + 12, STR3_CODE + 12, STR3_CODE + 13, STR3_CODE + 13, STR3_CODE + 14,
    STR3_CODE + 14, EOB_CODE, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 14,
    /* Padding: unused code slots filled with NONSENSE_CODE */
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE,
    NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE, NONSENSE_CODE};

/* Distance base values */
static const uint32_t arjz_cpdist[] = {
    0,          0,          0,          0,           1,          2,          3,          4,          5,
    7,          9,          13,         17,          25,         33,         49,         65,         97,
    129,        193,        257,        385,         513,        769,        1025,       1537,       2049,
    3073,       4097,       6145,       8193,        12289,      16385,      24577,      0x8001,     0xC001,
    0x00010001, 0x00018001, 0x00020001, 0x00030001,  0x00040001, 0x00060001, 0x00080001, 0x000C0001, 0x00100001,
    0x00180001, 0x00200001, 0x00300001, 0x00400001,  0x00600001, 0x00800001, 0x00C00001, 0x01000001, 0x01800001,
    0x02000001, 0x03000001, 0x04000001, 0x06000001,  0x08000001, 0x0C000001, 0x10000001, 0x18000001, 0x20000001,
    0x30000001, 0x40000001, 0x60000001, 0x80000001U, 0xC0000001U};

/* Distance extra bits */
static const uint32_t arjz_cpdext[] = {REPDIST_CODE,
                                       REPDIST_CODE + 1,
                                       REPDIST_CODE + 2,
                                       REPDIST_CODE + 3,
                                       0,
                                       0,
                                       0,
                                       0,
                                       1,
                                       1,
                                       2,
                                       2,
                                       3,
                                       3,
                                       4,
                                       4,
                                       5,
                                       5,
                                       6,
                                       6,
                                       7,
                                       7,
                                       8,
                                       8,
                                       9,
                                       9,
                                       10,
                                       10,
                                       11,
                                       11,
                                       12,
                                       12,
                                       13,
                                       13,
                                       14,
                                       14,
                                       15,
                                       15,
                                       16,
                                       16,
                                       17,
                                       17,
                                       18,
                                       18,
                                       19,
                                       19,
                                       20,
                                       20,
                                       21,
                                       21,
                                       22,
                                       22,
                                       23,
                                       23,
                                       24,
                                       24,
                                       25,
                                       25,
                                       26,
                                       26,
                                       27,
                                       27,
                                       28,
                                       28,
                                       29,
                                       29,
                                       30,
                                       30};

/* Bit masks */
static const uint32_t arjz_mask[] = {0x0000,     0x0001,     0x0003,     0x0007,     0x000f,     0x001f,     0x003f,
                                     0x007f,     0x00ff,     0x01ff,     0x03ff,     0x07ff,     0x0fff,     0x1fff,
                                     0x3fff,     0x7fff,     0xffff,     0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
                                     0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
                                     0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

/* ---- Bit I/O ---- */

static int arjz_nextbyte(arjz_ctx *ctx)
{
    if(ctx->in_pos >= ctx->in_len) return -1;
    return ctx->in_buf[ctx->in_pos++];
}

static int arjz_needbits(arjz_ctx *ctx, unsigned n)
{
    while(ctx->bk < n)
    {
        int c = arjz_nextbyte(ctx);
        if(c < 0) return -1;
        ctx->bb |= (uint32_t)c << ctx->bk;
        ctx->bk += 8;
    }
    return 0;
}

static unsigned arjz_readbits(arjz_ctx *ctx, unsigned n)
{
    unsigned val;
    arjz_needbits(ctx, n);
    val = (unsigned)(ctx->bb & arjz_mask[n]);
    ctx->bb >>= n;
    ctx->bk -= n;
    return val;
}

/* ---- Memory pool for Huffman tables ---- */

#define ARJZ_POOL_SIZE (256 * 1024)

static arjz_huft *arjz_pool_alloc(arjz_ctx *ctx, size_t count)
{
    size_t     need = count * sizeof(arjz_huft);
    arjz_huft *ptr;

    if(ctx->pool_used + need > ctx->pool_size) return NULL;

    ptr = (arjz_huft *)(ctx->pool + ctx->pool_used);
    ctx->pool_used += need;
    return ptr;
}

static void arjz_pool_reset(arjz_ctx *ctx) { ctx->pool_used = 0; }

/* ---- Huffman table builder ---- */

static int arjz_huft_build(arjz_ctx *ctx, unsigned *b, unsigned n, unsigned s, const uint32_t *d, const uint32_t *e,
                           arjz_huft **t, int *m)
{
    unsigned   a;
    unsigned   c[BMAX + 1];
    unsigned   el;
    unsigned   f;
    int        g;
    int        h;
    unsigned   i, j;
    int        k;
    int        lx[BMAX + 1];
    int       *l = lx + 1;
    unsigned  *p;
    arjz_huft *q;
    arjz_huft  r;
    arjz_huft *u[BMAX];
    unsigned   v[N_MAX];
    int        w;
    unsigned   x[BMAX + 1];
    unsigned  *xp;
    int        y;
    unsigned   z;

    el = n >= END_BLOCK_LIT ? b[END_BLOCK_LIT] : BMAX;
    memset(c, 0, sizeof(c));
    p = b;
    i = n;
    do
    {
        c[*p]++;
        p++;
    } while(--i);

    if(c[0] == n)
    {
        *t = NULL;
        *m = 0;
        return 0;
    }

    for(j = 1; j <= BMAX; j++)
        if(c[j]) break;
    k = (int)j;
    if((unsigned)*m < j) *m = (int)j;

    for(i = BMAX; i; i--)
        if(c[i]) break;
    g = (int)i;
    if((unsigned)*m > i) *m = (int)i;

    for(y = 1 << j; (int)j < (int)i; j++, y <<= 1)
        if((y -= (int)c[j]) < 0) return 2;
    if((y -= (int)c[i]) < 0) return 2;
    c[i] += (unsigned)y;

    x[1] = j = 0;
    p        = c + 1;
    xp       = x + 2;
    {
        unsigned ii = i;
        while(--ii) *xp++ = (j += *p++);
    }

    p = b;
    i = 0;
    do
    {
        if((j = *p++) != 0) v[x[j]++] = i;
    } while(++i < n);

    x[0] = i = 0;
    p        = v;
    h        = -1;
    w = l[-1] = 0;
    u[0]      = NULL;
    q         = NULL;
    z         = 0;

    for(; k <= g; k++)
    {
        a = c[k];
        while(a--)
        {
            while(k > w + l[h])
            {
                w += l[h++];
                z = (unsigned)(g - w);
                if(z > (unsigned)*m) z = (unsigned)*m;
                if((f = 1 << (j = (unsigned)(k - w))) > a + 1)
                {
                    f -= a + 1;
                    xp = c + k;
                    while(++j < z)
                    {
                        if((f <<= 1) <= *++xp) break;
                        f -= *xp;
                    }
                }
                if((unsigned)w + j > el && (unsigned)w < el) j = el - (unsigned)w;
                z    = 1 << j;
                l[h] = (int)j;

                q = arjz_pool_alloc(ctx, z + 1);
                if(!q) return 3;

                *t               = q + 1;
                *(t = &(q->v.t)) = NULL;
                u[h]             = ++q;

                if(h)
                {
                    x[h]        = i;
                    r.b         = (uint8_t)l[h - 1];
                    r.e         = (uint8_t)(TABLE_BASECODE + j);
                    r.v.t       = q;
                    j           = (i & ((1 << w) - 1)) >> (w - l[h - 1]);
                    u[h - 1][j] = r;
                }
            }

            r.b = (uint8_t)(k - w);
            if(p >= v + n)
                r.e = NONSENSE_CODE;
            else if(*p < s)
            {
                r.e   = LIT_CODE;
                r.v.n = *p++;
            }
            else
            {
                r.e   = (uint8_t)e[*p - s];
                r.v.n = d[*p++ - s];
            }

            f = 1 << (k - w);
            for(j = i >> w; j < z; j += f) q[j] = r;

            for(j = 1 << (k - 1); i & j; j >>= 1) i ^= j;
            i ^= j;

            while((i & ((1 << w) - 1)) != x[h]) w -= l[--h];
        }
    }

    *m = l[0];
    return y != 0 && g != 1;
}

/* ---- Delta encoding (multimedia) ---- */

static void arjz_undiff1(uint8_t *buf, unsigned items, unsigned step)
{
    uint8_t  prev = 0;
    unsigned i;
    for(i = 0; i < items; i++)
    {
        prev += buf[i * step];
        buf[i * step] = prev;
    }
}

static void arjz_undiff2(uint8_t *buf, unsigned items, unsigned step)
{
    int16_t  prev = 0;
    unsigned i;
    for(i = 0; i < items; i++)
    {
        int16_t val;
        memcpy(&val, buf + i * step, 2);
        prev += val;
        memcpy(buf + i * step, &prev, 2);
    }
}

static void arjz_undiff4(uint8_t *buf, unsigned items, unsigned step)
{
    int32_t  prev = 0;
    unsigned i;
    for(i = 0; i < items; i++)
    {
        int32_t val;
        memcpy(&val, buf + i * step, 4);
        prev += val;
        memcpy(buf + i * step, &prev, 4);
    }
}

static void arjz_diff1(uint8_t *buf, unsigned items, unsigned step)
{
    uint8_t  cur, prev = 0;
    unsigned i;
    for(i = 0; i < items; i++)
    {
        cur           = buf[i * step];
        buf[i * step] = cur - prev;
        prev          = cur;
    }
}

static void arjz_diff2(uint8_t *buf, unsigned items, unsigned step)
{
    int16_t  cur, prev = 0;
    unsigned i;
    for(i = 0; i < items; i++)
    {
        memcpy(&cur, buf + i * step, 2);
        int16_t diff = cur - prev;
        memcpy(buf + i * step, &diff, 2);
        prev = cur;
    }
}

static void arjz_diff4(uint8_t *buf, unsigned items, unsigned step)
{
    int32_t  cur, prev = 0;
    unsigned i;
    for(i = 0; i < items; i++)
    {
        memcpy(&cur, buf + i * step, 4);
        int32_t diff = cur - prev;
        memcpy(buf + i * step, &diff, 4);
        prev = cur;
    }
}

/* ---- E8/E9 post-processing ---- */

static int32_t arjz_untranslate(int32_t n, int32_t pos, int32_t fsize)
{
    if(n > 0 && n < fsize)
        n -= pos;
    else if(n <= 0 && n > -pos)
        n += fsize - 1;
    return n;
}

static void arjz_e8_process(arjz_ctx *ctx, uint8_t *buf, size_t len)
{
    size_t i = 0;

    while(i < len)
    {
        if(buf[i] == 0xE8 || buf[i] == 0xE9)
        {
            if(i + 5 <= len)
            {
                int32_t pos = (int32_t)(ctx->filepos + i + 5);
                int32_t val;
                memcpy(&val, buf + i + 1, 4);
                val = arjz_untranslate(val, pos, (int32_t)ctx->filesize);
                memcpy(buf + i + 1, &val, 4);
                i += 5;
                continue;
            }
        }
        i++;
    }
}

/* ---- MM block write-out ---- */

static void arjz_apply_mm(arjz_ctx *ctx, uint8_t *buf, size_t start, size_t bytes)
{
    if(ctx->mmcount > 0 && start < bytes)
    {
        uint8_t *mm_start = buf + start;
        unsigned cnt      = (bytes - start) / ctx->mmdist + 1;

        if(cnt <= 1) return;
        if(cnt > ctx->mmcount) cnt = ctx->mmcount;

        /* Save delta values before undiff */
        char mmtmp[MAX_MMDIST];
        memcpy(mmtmp, mm_start, ctx->mmdist);

        if(ctx->mmtype == 2) memcpy(mm_start, ctx->mmvalue, ctx->mmdist);

        switch(ctx->mmdist)
        {
            case 1:
                arjz_undiff1(mm_start, cnt, ctx->mmdist);
                break;
            case 2:
                arjz_undiff2(mm_start, cnt, ctx->mmdist);
                break;
            case 3:
                arjz_undiff1(mm_start, cnt, ctx->mmdist);
                arjz_undiff1(mm_start + 1, cnt, ctx->mmdist);
                arjz_undiff1(mm_start + 2, cnt, ctx->mmdist);
                break;
            case 4:
                arjz_undiff4(mm_start, cnt, ctx->mmdist);
                break;
        }

        /* Save partial sum for continuation */
        memcpy(ctx->mmvalue, mm_start + (cnt - 2) * ctx->mmdist, ctx->mmdist);

        /* Re-apply diff for continued processing */
        switch(ctx->mmdist)
        {
            case 1:
                arjz_diff1(mm_start, cnt, ctx->mmdist);
                break;
            case 2:
                arjz_diff2(mm_start, cnt, ctx->mmdist);
                break;
            case 3:
                arjz_diff1(mm_start, cnt, ctx->mmdist);
                arjz_diff1(mm_start + 1, cnt, ctx->mmdist);
                arjz_diff1(mm_start + 2, cnt, ctx->mmdist);
                break;
            case 4:
                arjz_diff4(mm_start, cnt, ctx->mmdist);
                break;
        }

        memcpy(mm_start, mmtmp, ctx->mmdist);

        if((bytes - start) / ctx->mmdist >= ctx->mmcount)
            ctx->mmcount = 0;
        else
        {
            ctx->mmcount -= cnt - 2;
            ctx->mmstart += (cnt - 2) * ctx->mmdist;
            ctx->mmtype = 2;
        }
    }
}

/* ---- Flush window to output ---- */

static void arjz_flush(arjz_ctx *ctx, size_t end)
{
    size_t   count = end - ctx->lastwp;
    uint8_t *src   = ctx->window + ctx->lastwp;

    /* Apply MM block processing */
    if(ctx->mmcount)
    {
        unsigned mm_rel = 0;
        if(ctx->mmstart >= ctx->lastwp) mm_rel = ctx->mmstart - ctx->lastwp;
        arjz_apply_mm(ctx, src, mm_rel, count);
    }

    /* Apply E8/E9 processing */
    if(ctx->exediff) arjz_e8_process(ctx, src, count);

    /* Copy to output */
    if(ctx->out_pos + count > ctx->out_len) count = ctx->out_len - ctx->out_pos;

    memcpy(ctx->out_buf + ctx->out_pos, src, count);
    ctx->out_pos += count;
    ctx->filepos += count;
    ctx->lastwp = end;
}

/* ---- Window management ---- */

static void arjz_output_char(arjz_ctx *ctx, size_t *wp, uint8_t c)
{
    ctx->window[*wp & ctx->window_mask] = c;
    (*wp)++;

    if(*wp >= ctx->window_size)
    {
        arjz_flush(ctx, ctx->window_size);
        *wp          = 0;
        ctx->lastwp  = 0;
        ctx->mmstart = 0;
    }
}

static void arjz_output_string(arjz_ctx *ctx, size_t *wp, uint32_t dist, unsigned len)
{
    unsigned i;

    for(i = 0; i < len; i++)
    {
        size_t  src                         = (*wp - (size_t)dist) & ctx->window_mask;
        uint8_t byte                        = ctx->window[src];
        ctx->window[*wp & ctx->window_mask] = byte;
        (*wp)++;

        if(*wp >= ctx->window_size)
        {
            arjz_flush(ctx, ctx->window_size);
            *wp          = 0;
            ctx->lastwp  = 0;
            ctx->mmstart = 0;
        }
    }
}

/* ---- Inflate stored block ---- */

static int arjz_inflate_stored(arjz_ctx *ctx)
{
    unsigned n;
    size_t   w;

    arjz_readbits(ctx, ctx->bk & 7); /* align to byte boundary */
    n = arjz_readbits(ctx, 16);

    if(n != (arjz_readbits(ctx, 16) ^ 0xffff)) return -1;

    w = ctx->wp;

    while(n--) { arjz_output_char(ctx, &w, (uint8_t)arjz_readbits(ctx, 8)); }

    ctx->wp = w;
    return 0;
}

/* ---- Inflate coded block ---- */

static int arjz_inflate_codes(arjz_ctx *ctx, arjz_huft *tl, arjz_huft *td, int bl, int bd)
{
    unsigned   e;
    unsigned   n;
    uint32_t   o;
    size_t     w;
    arjz_huft *t;
    unsigned   ml, md;

    uint32_t lastdist[REPDIST_CODES];
    unsigned lastptr;

    w  = ctx->wp;
    ml = arjz_mask[bl];
    md = arjz_mask[bd];

    lastptr = 0;
    for(n = 0; n < REPDIST_CODES; n++) lastdist[n] = 1;
    n = 1;
    o = 1;

    for(;;)
    {
        if(arjz_needbits(ctx, (unsigned)bl) < 0) return -1;
        t = tl + ((unsigned)ctx->bb & ml);
        e = t->e;

        while(e > TABLE_BASECODE)
        {
            if(e == NONSENSE_CODE) return -1;
            ctx->bb >>= t->b;
            ctx->bk -= t->b;
            e -= TABLE_BASECODE;
            if(arjz_needbits(ctx, e) < 0) return -1;
            t = t->v.t + ((unsigned)ctx->bb & arjz_mask[e]);
            e = t->e;
        }

        ctx->bb >>= t->b;
        ctx->bk -= t->b;

        if(e == LIT_CODE) { arjz_output_char(ctx, &w, (uint8_t)t->v.n); }
        else if(e == EOB_CODE) { break; }
        else if(e == MM_CODE)
        {
            /* Flush before MM block */
            if(ctx->mmcount || ctx->exediff) arjz_flush(ctx, w);

            if(arjz_needbits(ctx, 2) < 0) return -1;
            ctx->mmdist = 1 + ((unsigned)ctx->bb & 3);
            ctx->bb >>= 2;
            ctx->bk -= 2;
            ctx->mmstart = w - ctx->mmdist;

            /* Decode MM count using distance table */
            if(arjz_needbits(ctx, (unsigned)bd) < 0) return -1;
            t = td + ((unsigned)ctx->bb & md);
            e = t->e;
            while(e > TABLE_BASECODE)
            {
                if(e == NONSENSE_CODE) return -1;
                ctx->bb >>= t->b;
                ctx->bk -= t->b;
                e -= TABLE_BASECODE;
                if(arjz_needbits(ctx, e) < 0) return -1;
                t = t->v.t + ((unsigned)ctx->bb & arjz_mask[e]);
                e = t->e;
            }
            ctx->bb >>= t->b;
            ctx->bk -= t->b;

            if(arjz_needbits(ctx, e) < 0) return -1;
            ctx->mmcount = t->v.n + ((unsigned)ctx->bb & arjz_mask[e]);
            ctx->bb >>= e;
            ctx->bk -= e;

            ctx->mmtype = 1;
        }
        else
        {
            /* String/match codes */
            if(e >= STR2_CODE && e <= STR2_LASTCODE)
            {
                e -= STR2_CODE;
                if(e > 0)
                {
                    if(arjz_needbits(ctx, e) < 0) return -1;
                }
                o = t->v.n + (e > 0 ? ((unsigned)ctx->bb & arjz_mask[e]) : 0);
                if(e > 0)
                {
                    ctx->bb >>= e;
                    ctx->bk -= e;
                }
                n = 2;
            }
            else if(e >= REPDIST_CODE && e <= REPDIST_LASTCODE)
            {
                e -= REPDIST_CODE;
                o = lastdist[(lastptr - e) % REPDIST_CODES];
                n = 3;
                if(!e) goto skip_save;
            }
            else if(e >= STR3_CODE && e <= STR3_LASTCODE)
            {
                e -= STR3_CODE;
                if(e > 0)
                {
                    if(arjz_needbits(ctx, e) < 0) return -1;
                }
                o = t->v.n + (e > 0 ? ((unsigned)ctx->bb & arjz_mask[e]) : 0);
                if(e > 0)
                {
                    ctx->bb >>= e;
                    ctx->bk -= e;
                }
                n = 3;
            }
            else if(e == REPB_CODE)
            {
                /* Repeat both last length and offset */
                goto skip_save;
            }
            else
            {
                /* Standard length+distance */
                if(arjz_needbits(ctx, e) < 0) return -1;
                n = t->v.n + ((unsigned)ctx->bb & arjz_mask[e]);
                ctx->bb >>= e;
                ctx->bk -= e;

                /* Decode distance */
                if(arjz_needbits(ctx, (unsigned)bd) < 0) return -1;
                t = td + ((unsigned)ctx->bb & md);
                e = t->e;
                while(e > TABLE_BASECODE)
                {
                    if(e == NONSENSE_CODE) return -1;
                    ctx->bb >>= t->b;
                    ctx->bk -= t->b;
                    e -= TABLE_BASECODE;
                    if(arjz_needbits(ctx, e) < 0) return -1;
                    t = t->v.t + ((unsigned)ctx->bb & arjz_mask[e]);
                    e = t->e;
                }
                ctx->bb >>= t->b;
                ctx->bk -= t->b;

                if(e >= REPDIST_CODE)
                {
                    e -= REPDIST_CODE;
                    o = lastdist[(lastptr - e) % REPDIST_CODES];
                    if(!e) goto skip_save;
                }
                else
                {
                    o = t->v.n;
                    if(e > 25)
                    {
                        if(arjz_needbits(ctx, 16) < 0) return -1;
                        o += ((unsigned)ctx->bb & 0xFFFF);
                        ctx->bb >>= 16;
                        ctx->bk -= 16;
                        e -= 16;
                    }
                    if(e > 0)
                    {
                        if(arjz_needbits(ctx, e) < 0) return -1;
                        if(e <= 16)
                            o += ((unsigned)ctx->bb & arjz_mask[e]);
                        else
                            o += ((uint32_t)ctx->bb & arjz_mask[e]);
                        ctx->bb >>= e;
                        ctx->bk -= e;
                    }
                }
            }

            lastptr           = (lastptr + 1) % REPDIST_CODES;
            lastdist[lastptr] = o;

        skip_save:
        {
            unsigned cnt = n;
            if(o > ctx->md4) cnt++;
            arjz_output_string(ctx, &w, o, cnt);
        }
        }
    }

    ctx->wp = w;
    return 0;
}

/* ---- Inflate packed (dynamic or static Huffman) block ---- */

static int arjz_inflate_packed(arjz_ctx *ctx, int is_static)
{
    unsigned   i, j, l, n;
    unsigned   m;
    arjz_huft *tl = NULL;
    arjz_huft *td = NULL;
    unsigned   lll[BL_CODES];
    int        bl, bd;
    unsigned   nb, nl, nd;
    int        ret;

    arjz_pool_reset(ctx);

    if(is_static)
    {
        unsigned sl[512];
        for(i = 0; i < 512; i++) sl[i] = 9;
        bl  = 9;
        ret = arjz_huft_build(ctx, sl, 512, LITERALS, arjz_cplens, arjz_cplext, &tl, &bl);
        if(ret != 0 && ret != 1) return -1;

        for(i = 0; i < D_CODES; i++) sl[i] = D_CODES_LB;
        bd  = D_CODES_LB;
        ret = arjz_huft_build(ctx, sl, D_CODES, 0, arjz_cpdist, arjz_cpdext, &td, &bd);
        if(ret > 1) return -1;

        ret = arjz_inflate_codes(ctx, tl, td, bl, bd);
        return ret;
    }

    nl = LITERALS + SPEC_CODES + arjz_readbits(ctx, 5);
    nd = 1 + arjz_readbits(ctx, D_CODES_LB);
    nb = 4 + arjz_readbits(ctx, 4);

    if(nl > L_CODES || nd > D_CODES) return -1;

    memset(lll, 0, sizeof(lll));
    for(j = 0; j < nb; j++) lll[arjz_border[j]] = arjz_readbits(ctx, 3);

    bl  = 7;
    ret = arjz_huft_build(ctx, lll, BL_CODES, BL_CODES, NULL, NULL, &tl, &bl);
    if(ret != 0 && ret != 1) return -1;

    n = L_CODES + nd;
    m = arjz_mask[bl];
    i = 0;
    l = 0;

    while(i < n)
    {
        if(i == nl)
        {
            while(i < L_CODES) ctx->ll[i++] = 0;
        }

        if(arjz_needbits(ctx, (unsigned)bl) < 0) return -1;
        td = tl + ((unsigned)ctx->bb & m);
        j  = td->b;
        ctx->bb >>= j;
        ctx->bk -= j;
        j = td->v.n;

        if(j < 16) { ctx->ll[i++] += l = j; }
        else if(j == 16)
        {
            j = 3 + arjz_readbits(ctx, 2);
            if(i + j > n) return -1;
            while(j--) ctx->ll[i++] += l;
        }
        else if(j == 17)
        {
            j = 3 + arjz_readbits(ctx, 3);
            if(i + j > n) return -1;
            while(j--) ctx->ll[i++] += 0;
            l = 0;
        }
        else
        {
            j = 11 + arjz_readbits(ctx, 7);
            if(i + j > n) return -1;
            while(j--) ctx->ll[i++] += 0;
            l = 0;
        }
    }

    while(i < L_CODES + D_CODES) ctx->ll[i++] = 0;

    for(i = 0; i < L_CODES + D_CODES; i++) ctx->ll[i] &= 15;

    arjz_pool_reset(ctx);

    bl  = 10;
    ret = arjz_huft_build(ctx, ctx->ll, nl, LITERALS, arjz_cplens, arjz_cplext, &tl, &bl);
    if(ret != 0 && ret != 1) return -1;

    bd  = D_CODES_LB;
    ret = arjz_huft_build(ctx, ctx->ll + L_CODES, nd, 0, arjz_cpdist, arjz_cpdext, &td, &bd);
    if(ret != 0 && ret != 1) return -1;

    ret = arjz_inflate_codes(ctx, tl, td, bl, bd);
    return ret;
}

/* ---- Main inflate entry point ---- */

AARU_EXPORT int AARU_CALL arjz_decompress_buffer(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len, size_t orig_size)
{
    arjz_ctx ctx;
    int      last_block;
    int      byte0, byte1;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    memset(&ctx, 0, sizeof(ctx));
    ctx.in_buf   = in_buf;
    ctx.in_len   = in_len;
    ctx.out_buf  = out_buf;
    ctx.out_len  = *out_len;
    ctx.filesize = orig_size;

    /* Compute window size: smallest power-of-2 >= orig_size, minimum 64KB */
    {
        size_t ws = ARJZ_MIN_WINDOW_SIZE;
        while(ws < orig_size && ws < (size_t)1 << 30) ws <<= 1;
        ctx.window_size = ws;
        ctx.window_mask = ws - 1;
    }

    ctx.window = (uint8_t *)calloc(ctx.window_size + MAX_MMDIST * 2, 1);
    if(!ctx.window) return -1;

    ctx.pool_size = ARJZ_POOL_SIZE;
    ctx.pool      = (uint8_t *)malloc(ctx.pool_size);
    if(!ctx.pool)
    {
        free(ctx.window);
        return -1;
    }

    /* Read stream header: md4 parameter and exediff flag */
    byte0 = (int)arjz_readbits(&ctx, 8);
    byte1 = (int)arjz_readbits(&ctx, 8);

    if(--byte0 < 0)
        ctx.md4 = (uint32_t)byte1;
    else
        ctx.md4 = (uint32_t)(byte1 + 256) << byte0;

    ctx.exediff = arjz_readbits(&ctx, 1);

    memset(ctx.ll, 0, sizeof(ctx.ll));

    do
    {
        last_block          = (int)arjz_readbits(&ctx, 1);
        unsigned block_type = arjz_readbits(&ctx, 2);
        int      ret;

        switch(block_type)
        {
            case 0:
                ret = arjz_inflate_stored(&ctx);
                break;
            case 1:
                ret = arjz_inflate_packed(&ctx, 1);
                break;
            case 2:
                ret = arjz_inflate_packed(&ctx, 0);
                break;
            default:
                ret = -1;
                break;
        }

        if(ret != 0)
        {
            free(ctx.pool);
            free(ctx.window);
            return -1;
        }
    } while(!last_block);

    /* Flush remaining data */
    if(ctx.wp != ctx.lastwp) arjz_flush(&ctx, ctx.wp);

    *out_len = ctx.out_pos;
    free(ctx.pool);
    free(ctx.window);
    return 0;
}
