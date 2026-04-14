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
 * Self-contained port of LH2, LH3 and PM2 decompression from legacy code.
 * Uses its own 16-bit bit I/O and Huffman tree management.
 */

#include "lh_old.h"

#include <stdlib.h>
#include <string.h>

/* --- Constants --- */

#define UCHAR_MAX_VAL   255
#define CHAR_BIT_VAL    8
#define USHRT_BIT       16
#define MAXMATCH        256
#define NC              (UCHAR_MAX_VAL + MAXMATCH + 2 - THRESHOLD)
#define THRESHOLD       3
#define NPT             0x80
#define CBIT            9
#define TBIT            5
#define NT              (USHRT_BIT + 3)
#define N_CHAR          (256 + 60 - THRESHOLD + 1)
#define TREESIZE_C      (N_CHAR * 2)
#define TREESIZE_P      (128 * 2)
#define TREESIZE        (TREESIZE_C + TREESIZE_P)
#define ROOT_C          0
#define ROOT_P          TREESIZE_C
#define N1              286
#define EXTRABITS       8
#define BUFBITS         16
#define NP              (MAX_DICBIT + 1)
#define MAX_DICBIT      16
#define LENFIELD        4
#define PMARC2_OFFSET   (0x100 - 2)

/* --- Buffer I/O --- */

typedef struct
{
    const uint8_t *in_buf;
    size_t         in_len;
    size_t         in_pos;
    uint8_t       *out_buf;
    size_t         out_len;
    size_t         out_pos;
    int            error;
} lhold_io;

static int lhold_getchar(lhold_io *io)
{
    if(io->in_pos >= io->in_len) return 0;

    return io->in_buf[io->in_pos++];
}

static void lhold_putchar(lhold_io *io, uint8_t c)
{
    if(io->out_pos < io->out_len) io->out_buf[io->out_pos++] = c;
}

static int lhold_at_end(lhold_io *io) { return io->out_pos >= io->out_len; }

/* --- PM2 Tree --- */

struct pmarc2_tree
{
    uint8_t *leftarr;
    uint8_t *rightarr;
    uint8_t  root;
};

/* --- Decompression data --- */

struct lhold_st
{
    int32_t  pbit;
    int32_t  np;
    int32_t  nn;
    int32_t  n1;
    int32_t  most_p;
    int32_t  avail;
    uint32_t n_max;
    uint16_t maxmatch;
    uint16_t total_p;
    uint16_t blocksize;
    uint16_t c_table[4096];
    uint16_t pt_table[256];
    uint16_t left[2 * NC - 1];
    uint16_t right[2 * NC - 1];
    uint16_t freq[TREESIZE];
    uint16_t pt_code[NPT];
    int16_t  child[TREESIZE];
    int16_t  stock[TREESIZE];
    int16_t  s_node[TREESIZE / 2];
    int16_t  block[TREESIZE];
    int16_t  parent[TREESIZE];
    int16_t  edge[TREESIZE];
    uint8_t  c_len[NC];
    uint8_t  pt_len[NPT];
};

struct lhold_pm
{
    struct pmarc2_tree tree1;
    struct pmarc2_tree tree2;
    uint16_t lastupdate;
    uint16_t dicsiz1;
    uint8_t  gettree1;
    uint8_t  tree1left[32];
    uint8_t  tree1right[32];
    uint8_t  table1[32];
    uint8_t  tree2left[8];
    uint8_t  tree2right[8];
    uint8_t  table2[8];
    uint8_t  tree1bound;
    uint8_t  mindepth;
    uint8_t  prev[0x100];
    uint8_t  next[0x100];
    uint8_t  parentarr[0x100];
    uint8_t  lastbyte;
};

struct lhold_data
{
    lhold_io *io;
    uint8_t  *text;
    uint16_t  dicbit;

    uint16_t bitbuf;
    uint8_t  subbitbuf;
    uint8_t  bitcount;
    uint32_t loc;
    uint32_t count;
    uint32_t nextcount;

    union
    {
        struct lhold_st st;
        struct lhold_pm pm;
    } d;
};

/* --- Bit I/O (16-bit accumulator) --- */

static void fillbuf(struct lhold_data *dat, uint8_t n)
{
    if(dat->io->error) return;

    while(n > dat->bitcount)
    {
        n -= dat->bitcount;
        dat->bitbuf = (uint16_t)((dat->bitbuf << dat->bitcount) + (dat->subbitbuf >> (CHAR_BIT_VAL - dat->bitcount)));
        dat->subbitbuf = (uint8_t)lhold_getchar(dat->io);
        dat->bitcount  = CHAR_BIT_VAL;
    }

    dat->bitcount -= n;
    dat->bitbuf = (uint16_t)((dat->bitbuf << n) + (dat->subbitbuf >> (CHAR_BIT_VAL - n)));
    dat->subbitbuf <<= n;
}

static uint16_t getbits(struct lhold_data *dat, uint8_t n)
{
    uint16_t x = (uint16_t)(dat->bitbuf >> (2 * CHAR_BIT_VAL - n));

    fillbuf(dat, n);
    return x;
}

#define init_getbits(a) fillbuf((a), 2 * CHAR_BIT_VAL)

/* --- Huffman table construction --- */

static void make_table(struct lhold_data *dat, int16_t nchar, uint8_t bitlen[], int16_t tablebits, uint16_t table[])
{
    uint16_t count[17];
    uint16_t weight[17];
    uint16_t start[17];
    uint16_t total;
    uint32_t i;
    int32_t  j, k, l, m, n, avail;
    uint16_t *p;

    if(dat->io->error) return;

    avail = nchar;
    memset(count, 0, sizeof(count));

    for(i = 1; i <= 16; i++) weight[i] = (uint16_t)(1 << (16 - i));

    for(i = 0; (int32_t)i < nchar; i++) count[bitlen[i]]++;

    total = 0;

    for(i = 1; i <= 16; i++)
    {
        start[i] = total;
        total = (uint16_t)(total + weight[i] * count[i]);
    }

    if(total & 0xFFFF)
    {
        dat->io->error = 1;
        return;
    }

    m = 16 - tablebits;

    for(i = 1; (int32_t)i <= tablebits; i++)
    {
        start[i] >>= m;
        weight[i] >>= m;
    }

    j = start[tablebits + 1] >> m;
    k = 1 << tablebits;

    if(j != 0)
        for(i = (uint32_t)j; (int32_t)i < k; i++) table[i] = 0;

    for(j = 0; j < nchar; j++)
    {
        k = bitlen[j];

        if(k == 0) continue;

        l = start[k] + weight[k];

        if(k <= tablebits)
        {
            for(i = (uint32_t)start[k]; (int32_t)i < l; i++) table[i] = (uint16_t)j;
        }
        else
        {
            p = &table[(i = (uint32_t)start[k]) >> m];
            i <<= tablebits;
            n = k - tablebits;

            while(--n >= 0)
            {
                if(*p == 0)
                {
                    dat->d.st.right[avail] = dat->d.st.left[avail] = 0;
                    *p = (uint16_t)avail++;
                }

                if(i & 0x8000)
                    p = &dat->d.st.right[*p];
                else
                    p = &dat->d.st.left[*p];

                i <<= 1;
            }

            *p = (uint16_t)j;
        }

        start[k] = (uint16_t)l;
    }
}

/* --- LH2: Dynamic Huffman --- */

static void start_p_dyn(struct lhold_data *dat)
{
    dat->d.st.freq[ROOT_P]  = 1;
    dat->d.st.child[ROOT_P] = (int16_t)(~(N_CHAR));
    dat->d.st.s_node[N_CHAR] = ROOT_P;
    dat->d.st.edge[dat->d.st.block[ROOT_P] = dat->d.st.stock[dat->d.st.avail++]] = ROOT_P;
    dat->d.st.most_p  = ROOT_P;
    dat->d.st.total_p = 0;
    dat->d.st.nn      = 1 << dat->dicbit;
    dat->nextcount     = 64;
}

static void start_c_dyn(struct lhold_data *dat)
{
    int32_t i, j, f;

    dat->d.st.n1 = (int32_t)((dat->d.st.n_max >= (uint32_t)(256 + dat->d.st.maxmatch - THRESHOLD + 1)) ? 512
                                                                                                         : dat->d.st.n_max - 1);

    for(i = 0; i < TREESIZE_C; i++)
    {
        dat->d.st.stock[i] = (int16_t)i;
        dat->d.st.block[i] = 0;
    }

    for(i = 0, j = (int32_t)(dat->d.st.n_max * 2 - 2); i < (int32_t)dat->d.st.n_max; i++, j--)
    {
        dat->d.st.freq[j]  = 1;
        dat->d.st.child[j] = (int16_t)(~i);
        dat->d.st.s_node[i] = (int16_t)j;
        dat->d.st.block[j] = 1;
    }

    dat->d.st.avail = 2;
    dat->d.st.edge[1] = (int16_t)(dat->d.st.n_max - 1);
    i = (int32_t)(dat->d.st.n_max * 2 - 2);

    while(j >= 0)
    {
        f = dat->d.st.freq[j] = dat->d.st.freq[i] + dat->d.st.freq[i - 1];
        dat->d.st.child[j] = (int16_t)i;
        dat->d.st.parent[i] = dat->d.st.parent[i - 1] = (int16_t)j;

        if(f == dat->d.st.freq[j + 1])
            dat->d.st.edge[dat->d.st.block[j] = dat->d.st.block[j + 1]] = (int16_t)j;
        else
            dat->d.st.edge[dat->d.st.block[j] = dat->d.st.stock[dat->d.st.avail++]] = (int16_t)j;

        i -= 2;
        j--;
    }
}

static void decode_start_dyn(struct lhold_data *dat)
{
    dat->d.st.n_max    = 286;
    dat->d.st.maxmatch = MAXMATCH;
    init_getbits(dat);
    start_c_dyn(dat);
    start_p_dyn(dat);
}

static void reconst(struct lhold_data *dat, int32_t start, int32_t end)
{
    int32_t  i, j, k, l, b = 0;
    uint32_t f, g;

    for(i = j = start; i < end; i++)
    {
        if((k = dat->d.st.child[i]) < 0)
        {
            dat->d.st.freq[j]  = (uint16_t)((dat->d.st.freq[i] + 1) / 2);
            dat->d.st.child[j] = (int16_t)k;
            j++;
        }

        if(dat->d.st.edge[b = dat->d.st.block[i]] == i) dat->d.st.stock[--dat->d.st.avail] = (int16_t)b;
    }

    j--;
    i = end - 1;
    l = end - 2;

    while(i >= start)
    {
        while(i >= l)
        {
            dat->d.st.freq[i]  = dat->d.st.freq[j];
            dat->d.st.child[i] = dat->d.st.child[j];
            i--;
            j--;
        }

        f = (uint32_t)(dat->d.st.freq[l] + dat->d.st.freq[l + 1]);

        for(k = start; f < (uint32_t)dat->d.st.freq[k]; k++)
            ;

        while(j >= k)
        {
            dat->d.st.freq[i]  = dat->d.st.freq[j];
            dat->d.st.child[i] = dat->d.st.child[j];
            i--;
            j--;
        }

        dat->d.st.freq[i]  = (uint16_t)f;
        dat->d.st.child[i] = (int16_t)(l + 1);
        i--;
        l -= 2;
    }

    f = 0;

    for(i = start; i < end; i++)
    {
        if((j = dat->d.st.child[i]) < 0)
            dat->d.st.s_node[~j] = (int16_t)i;
        else
            dat->d.st.parent[j] = dat->d.st.parent[j - 1] = (int16_t)i;

        if((g = (uint32_t)dat->d.st.freq[i]) == f)
            dat->d.st.block[i] = (int16_t)b;
        else
        {
            dat->d.st.edge[b = dat->d.st.block[i] = dat->d.st.stock[dat->d.st.avail++]] = (int16_t)i;
            f = g;
        }
    }
}

static int32_t swap_inc(struct lhold_data *dat, int32_t p)
{
    int32_t b, q, r, s;

    b = dat->d.st.block[p];

    if((q = dat->d.st.edge[b]) != p)
    {
        r = dat->d.st.child[p];
        s = dat->d.st.child[q];
        dat->d.st.child[p] = (int16_t)s;
        dat->d.st.child[q] = (int16_t)r;

        if(r >= 0)
            dat->d.st.parent[r] = dat->d.st.parent[r - 1] = (int16_t)q;
        else
            dat->d.st.s_node[~r] = (int16_t)q;

        if(s >= 0)
            dat->d.st.parent[s] = dat->d.st.parent[s - 1] = (int16_t)p;
        else
            dat->d.st.s_node[~s] = (int16_t)p;

        p = q;
        dat->d.st.edge[b]++;

        if(++dat->d.st.freq[p] == dat->d.st.freq[p - 1])
            dat->d.st.block[p] = dat->d.st.block[p - 1];
        else
            dat->d.st.edge[dat->d.st.block[p] = dat->d.st.stock[dat->d.st.avail++]] = (int16_t)p;
    }
    else if(b == dat->d.st.block[p + 1])
    {
        dat->d.st.edge[b]++;

        if(++dat->d.st.freq[p] == dat->d.st.freq[p - 1])
            dat->d.st.block[p] = dat->d.st.block[p - 1];
        else
            dat->d.st.edge[dat->d.st.block[p] = dat->d.st.stock[dat->d.st.avail++]] = (int16_t)p;
    }
    else if(++dat->d.st.freq[p] == dat->d.st.freq[p - 1])
    {
        dat->d.st.stock[--dat->d.st.avail] = (int16_t)b;
        dat->d.st.block[p]                  = dat->d.st.block[p - 1];
    }

    return dat->d.st.parent[p];
}

static void update_p(struct lhold_data *dat, int32_t p)
{
    int32_t q;

    if(dat->d.st.total_p == 0x8000)
    {
        reconst(dat, ROOT_P, dat->d.st.most_p + 1);
        dat->d.st.total_p = dat->d.st.freq[ROOT_P];
        dat->d.st.freq[ROOT_P] = 0xffff;
    }

    q = dat->d.st.s_node[p + N_CHAR];

    while(q != ROOT_P) q = swap_inc(dat, q);

    dat->d.st.total_p++;
}

static void make_new_node(struct lhold_data *dat, int32_t p)
{
    int32_t q, r;

    r = dat->d.st.most_p + 1;
    q = r + 1;
    dat->d.st.s_node[~(dat->d.st.child[r] = dat->d.st.child[dat->d.st.most_p])] = (int16_t)r;
    dat->d.st.child[q] = (int16_t)(~(p + N_CHAR));
    dat->d.st.child[dat->d.st.most_p] = (int16_t)q;
    dat->d.st.freq[r]  = dat->d.st.freq[dat->d.st.most_p];
    dat->d.st.freq[q]  = 0;
    dat->d.st.block[r]  = dat->d.st.block[dat->d.st.most_p];

    if(dat->d.st.most_p == ROOT_P)
    {
        dat->d.st.freq[ROOT_P] = 0xffff;
        dat->d.st.edge[dat->d.st.block[ROOT_P]]++;
    }

    dat->d.st.parent[r] = dat->d.st.parent[q] = (int16_t)dat->d.st.most_p;
    dat->d.st.edge[dat->d.st.block[q] = dat->d.st.stock[dat->d.st.avail++]] = dat->d.st.s_node[p + N_CHAR] =
        (int16_t)(dat->d.st.most_p = q);
    update_p(dat, p);
}

static void update_c(struct lhold_data *dat, int32_t p)
{
    int32_t q;

    if(dat->d.st.freq[ROOT_C] == 0x8000) reconst(dat, 0, (int32_t)dat->d.st.n_max * 2 - 1);

    dat->d.st.freq[ROOT_C]++;
    q = dat->d.st.s_node[p];

    do { q = swap_inc(dat, q); } while(q != ROOT_C);
}

static uint16_t decode_c_dyn(struct lhold_data *dat)
{
    int32_t c;
    int16_t buf, cnt;

    c   = dat->d.st.child[ROOT_C];
    buf = (int16_t)dat->bitbuf;
    cnt = 0;

    do
    {
        c = dat->d.st.child[c - (buf < 0)];
        buf <<= 1;

        if(++cnt == 16)
        {
            fillbuf(dat, 16);
            buf = (int16_t)dat->bitbuf;
            cnt = 0;
        }
    } while(c > 0);

    fillbuf(dat, (uint8_t)cnt);
    c = ~c;
    update_c(dat, c);

    if(c == dat->d.st.n1) c += getbits(dat, 8);

    return (uint16_t)c;
}

static uint16_t decode_p_dyn(struct lhold_data *dat)
{
    int32_t c;
    int16_t buf, cnt;

    while(dat->count > dat->nextcount)
    {
        make_new_node(dat, (int32_t)dat->nextcount / 64);

        if((dat->nextcount += 64) >= (uint32_t)dat->d.st.nn) dat->nextcount = 0xffffffff;
    }

    c   = dat->d.st.child[ROOT_P];
    buf = (int16_t)dat->bitbuf;
    cnt = 0;

    while(c > 0)
    {
        c = dat->d.st.child[c - (buf < 0)];
        buf <<= 1;

        if(++cnt == 16)
        {
            fillbuf(dat, 16);
            buf = (int16_t)dat->bitbuf;
            cnt = 0;
        }
    }

    fillbuf(dat, (uint8_t)cnt);
    c = (~c) - N_CHAR;
    update_p(dat, c);

    return (uint16_t)((c << 6) + getbits(dat, 6));
}

/* --- LH3: Static Huffman --- */

static const int32_t fixed_tables[2][16] = {{3, 0x01, 0x04, 0x0c, 0x18, 0x30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                             {2, 0x01, 0x01, 0x03, 0x06, 0x0D, 0x1F, 0x4E, 0, 0, 0, 0, 0, 0, 0, 0}};

static void ready_made(struct lhold_data *dat, int32_t method)
{
    int32_t        i, j;
    uint32_t       code, weight_val;
    const int32_t *tbl;

    tbl        = fixed_tables[method];
    j          = *tbl++;
    weight_val = (uint32_t)(1 << (16 - j));
    code       = 0;

    for(i = 0; i < dat->d.st.np; i++)
    {
        while(*tbl == i)
        {
            j++;
            tbl++;
            weight_val >>= 1;
        }

        dat->d.st.pt_len[i]  = (uint8_t)j;
        dat->d.st.pt_code[i] = (uint16_t)code;
        code += weight_val;
    }
}

static uint16_t decode_p_st0(struct lhold_data *dat)
{
    int32_t i, j;

    j = dat->d.st.pt_table[dat->bitbuf >> 8];

    if(j < dat->d.st.np)
    {
        fillbuf(dat, dat->d.st.pt_len[j]);
    }
    else
    {
        fillbuf(dat, 8);
        i = dat->bitbuf;

        do
        {
            if((int16_t)i < 0)
                j = dat->d.st.right[j];
            else
                j = dat->d.st.left[j];

            i <<= 1;
        } while(j >= dat->d.st.np);

        fillbuf(dat, (uint8_t)(dat->d.st.pt_len[j] - 8));
    }

    return (uint16_t)((j << 6) + getbits(dat, 6));
}

static void decode_start_st0(struct lhold_data *dat)
{
    dat->d.st.n_max    = 286;
    dat->d.st.maxmatch = MAXMATCH;
    init_getbits(dat);
    dat->d.st.np = 1 << (MAX_DICBIT - 6);
}

static void read_tree_c(struct lhold_data *dat)
{
    int32_t i, c;

    i = 0;

    while(i < N1)
    {
        if(getbits(dat, 1))
            dat->d.st.c_len[i] = (uint8_t)(getbits(dat, LENFIELD) + 1);
        else
            dat->d.st.c_len[i] = 0;

        if(++i == 3 && dat->d.st.c_len[0] == 1 && dat->d.st.c_len[1] == 1 && dat->d.st.c_len[2] == 1)
        {
            c = getbits(dat, CBIT);
            memset(dat->d.st.c_len, 0, N1);

            for(i = 0; i < 4096; i++) dat->d.st.c_table[i] = (uint16_t)c;

            return;
        }
    }

    make_table(dat, N1, dat->d.st.c_len, 12, dat->d.st.c_table);
}

static void read_tree_p(struct lhold_data *dat)
{
    int32_t i, c;

    i = 0;

    while(i < NP)
    {
        dat->d.st.pt_len[i] = (uint8_t)getbits(dat, LENFIELD);

        if(++i == 3 && dat->d.st.pt_len[0] == 1 && dat->d.st.pt_len[1] == 1 && dat->d.st.pt_len[2] == 1)
        {
            c = getbits(dat, MAX_DICBIT - 6);

            for(i = 0; i < NP; i++) dat->d.st.c_len[i] = 0;

            for(i = 0; i < 256; i++) dat->d.st.c_table[i] = (uint16_t)c;

            return;
        }
    }
}

static uint16_t decode_c_st0(struct lhold_data *dat)
{
    int32_t i, j;

    if(!dat->d.st.blocksize)
    {
        dat->d.st.blocksize = getbits(dat, BUFBITS);
        read_tree_c(dat);

        if(getbits(dat, 1))
            read_tree_p(dat);
        else
            ready_made(dat, 1);

        make_table(dat, NP, dat->d.st.pt_len, 8, dat->d.st.pt_table);
    }

    dat->d.st.blocksize--;
    j = dat->d.st.c_table[dat->bitbuf >> 4];

    if(j < N1)
    {
        fillbuf(dat, dat->d.st.c_len[j]);
    }
    else
    {
        fillbuf(dat, 12);
        i = dat->bitbuf;

        do
        {
            if((int16_t)i < 0)
                j = dat->d.st.right[j];
            else
                j = dat->d.st.left[j];

            i <<= 1;
        } while(j >= N1);

        fillbuf(dat, (uint8_t)(dat->d.st.c_len[j] - 12));
    }

    if(j == N1 - 1) j += getbits(dat, EXTRABITS);

    return (uint16_t)j;
}

/* --- PM2: PMarc v2 --- */

static const int32_t pmarc2_historyBits[8] = {3, 3, 4, 5, 5, 5, 6, 6};
static const int32_t pmarc2_historyBase[8] = {0, 8, 16, 32, 64, 96, 128, 192};
static const int32_t pmarc2_repeatBits[6]  = {3, 3, 5, 6, 7, 0};
static const int32_t pmarc2_repeatBase[6]  = {17, 25, 33, 65, 129, 256};

static void pmarc2_hist_update(struct lhold_data *dat, uint8_t data)
{
    if(data != dat->d.pm.lastbyte)
    {
        uint8_t oldNext, oldPrev, newNext;

        oldNext = dat->d.pm.next[data];
        oldPrev = dat->d.pm.prev[data];
        dat->d.pm.prev[oldNext] = oldPrev;
        dat->d.pm.next[oldPrev] = oldNext;

        newNext                  = dat->d.pm.next[dat->d.pm.lastbyte];
        dat->d.pm.prev[newNext]  = data;
        dat->d.pm.next[data]     = newNext;

        dat->d.pm.prev[data]                 = dat->d.pm.lastbyte;
        dat->d.pm.next[dat->d.pm.lastbyte] = data;

        dat->d.pm.lastbyte = data;
    }
}

static int32_t pmarc2_tree_get(struct lhold_data *dat, struct pmarc2_tree *t)
{
    int32_t i = t->root;

    while(i < 0x80) i = (getbits(dat, 1) == 0 ? t->leftarr[i] : t->rightarr[i]);

    return i & 0x7F;
}

static void pmarc2_tree_rebuild(struct lhold_data *dat, struct pmarc2_tree *t, uint8_t bound, uint8_t mindepth,
                                uint8_t *table)
{
    uint8_t d;
    int32_t i, curr, empty_node, n;

    t->root = 0;
    memset(t->leftarr, 0, bound);
    memset(t->rightarr, 0, bound);
    memset(dat->d.pm.parentarr, 0, bound);

    for(i = 0; i < dat->d.pm.mindepth - 1; i++)
    {
        t->leftarr[i] = (uint8_t)(i + 1);
        dat->d.pm.parentarr[i + 1] = (uint8_t)i;
    }

    curr       = dat->d.pm.mindepth - 1;
    empty_node = dat->d.pm.mindepth;

    for(d = dat->d.pm.mindepth;; d++)
    {
        for(i = 0; i < bound; i++)
        {
            if(table[i] == d)
            {
                if(t->leftarr[curr] == 0)
                {
                    t->leftarr[curr] = (uint8_t)(i | 128);
                }
                else
                {
                    t->rightarr[curr] = (uint8_t)(i | 128);
                    n                 = 0;

                    while(t->rightarr[curr] != 0)
                    {
                        if(curr == 0) return;

                        curr = dat->d.pm.parentarr[curr];
                        n++;
                    }

                    t->rightarr[curr] = (uint8_t)empty_node;

                    for(;;)
                    {
                        dat->d.pm.parentarr[empty_node] = (uint8_t)curr;
                        curr                             = empty_node;
                        empty_node++;
                        n--;

                        if(n == 0) break;

                        t->leftarr[curr] = (uint8_t)empty_node;
                    }
                }
            }
        }

        if(t->leftarr[curr] == 0)
            t->leftarr[curr] = (uint8_t)empty_node;
        else
            t->rightarr[curr] = (uint8_t)empty_node;

        dat->d.pm.parentarr[empty_node] = (uint8_t)curr;
        curr                             = empty_node;
        empty_node++;
    }
}

static uint8_t pmarc2_hist_lookup(struct lhold_data *dat, int32_t n)
{
    uint8_t  i;
    uint8_t *direction = dat->d.pm.prev;

    if(n >= 0x80)
    {
        direction = dat->d.pm.next;
        n         = 0x100 - n;
    }

    for(i = dat->d.pm.lastbyte; n != 0; n--) i = direction[i];

    return i;
}

static void pmarc2_maketree1(struct lhold_data *dat)
{
    int32_t i, nbits, x;

    dat->d.pm.tree1bound = (uint8_t)getbits(dat, 5);
    dat->d.pm.mindepth   = (uint8_t)getbits(dat, 3);

    if(dat->d.pm.mindepth == 0)
    {
        dat->d.pm.tree1.root = (uint8_t)(128 | (dat->d.pm.tree1bound - 1));
    }
    else
    {
        memset(dat->d.pm.table1, 0, 32);
        nbits = getbits(dat, 3);

        for(i = 0; i < dat->d.pm.tree1bound; i++)
        {
            if((x = getbits(dat, (uint8_t)nbits))) dat->d.pm.table1[i] = (uint8_t)(x - 1 + dat->d.pm.mindepth);
        }

        pmarc2_tree_rebuild(dat, &dat->d.pm.tree1, dat->d.pm.tree1bound, dat->d.pm.mindepth, dat->d.pm.table1);
    }
}

static void pmarc2_maketree2(struct lhold_data *dat, int32_t par_b)
{
    int32_t i, count, index;

    if(dat->d.pm.tree1bound < 10) return;

    if(dat->d.pm.tree1bound == 29 && dat->d.pm.mindepth == 0) return;

    for(i = 0; i < 8; i++) dat->d.pm.table2[i] = 0;

    for(i = 0; i < par_b; i++) dat->d.pm.table2[i] = (uint8_t)getbits(dat, 3);

    index = 0;
    count = 0;

    for(i = 0; i < 8; i++)
    {
        if(dat->d.pm.table2[i] != 0)
        {
            index = i;
            count++;
        }
    }

    if(count == 1)
        dat->d.pm.tree2.root = (uint8_t)(128 | index);
    else if(count > 1)
    {
        dat->d.pm.mindepth = 1;
        pmarc2_tree_rebuild(dat, &dat->d.pm.tree2, 8, dat->d.pm.mindepth, dat->d.pm.table2);
    }
}

static void decode_start_pm2(struct lhold_data *dat)
{
    int32_t i;

    dat->d.pm.tree1.leftarr  = dat->d.pm.tree1left;
    dat->d.pm.tree1.rightarr = dat->d.pm.tree1right;
    dat->d.pm.tree2.leftarr  = dat->d.pm.tree2left;
    dat->d.pm.tree2.rightarr = dat->d.pm.tree2right;

    dat->d.pm.dicsiz1 = (uint16_t)((1 << dat->dicbit) - 1);
    init_getbits(dat);

    /* Initialize history list */
    for(i = 0; i < 0x100; i++)
    {
        dat->d.pm.prev[(0xFF + i) & 0xFF] = (uint8_t)i;
        dat->d.pm.next[(0x01 + i) & 0xFF] = (uint8_t)i;
    }

    dat->d.pm.prev[0x7F] = 0x00;
    dat->d.pm.next[0x00] = 0x7F;
    dat->d.pm.prev[0xDF] = 0x80;
    dat->d.pm.next[0x80] = 0xDF;
    dat->d.pm.prev[0x9F] = 0xE0;
    dat->d.pm.next[0xE0] = 0x9F;
    dat->d.pm.prev[0x1F] = 0xA0;
    dat->d.pm.next[0xA0] = 0x1F;
    dat->d.pm.prev[0xFF] = 0x20;
    dat->d.pm.next[0x20] = 0xFF;
    dat->d.pm.lastbyte    = 0x20;

    getbits(dat, 1); /* discard bit */
}

static uint16_t decode_c_pm2(struct lhold_data *dat)
{
    while(dat->d.pm.lastupdate != dat->loc)
    {
        pmarc2_hist_update(dat, dat->text[dat->d.pm.lastupdate]);
        dat->d.pm.lastupdate = (uint16_t)((dat->d.pm.lastupdate + 1) & dat->d.pm.dicsiz1);
    }

    while(dat->count >= dat->nextcount)
    {
        if(dat->nextcount == 0x0000)
        {
            pmarc2_maketree1(dat);
            pmarc2_maketree2(dat, 5);
            dat->nextcount = 0x0400;
        }
        else if(dat->nextcount == 0x0400)
        {
            pmarc2_maketree2(dat, 6);
            dat->nextcount = 0x0800;
        }
        else if(dat->nextcount == 0x0800)
        {
            pmarc2_maketree2(dat, 7);
            dat->nextcount = 0x1000;
        }
        else if(dat->nextcount == 0x1000)
        {
            if(getbits(dat, 1) != 0) pmarc2_maketree1(dat);

            pmarc2_maketree2(dat, 8);
            dat->nextcount = 0x2000;
        }
        else
        {
            if(getbits(dat, 1) != 0)
            {
                pmarc2_maketree1(dat);
                pmarc2_maketree2(dat, 8);
            }

            dat->nextcount += 0x1000;
        }
    }

    dat->d.pm.gettree1 = (uint8_t)pmarc2_tree_get(dat, &dat->d.pm.tree1);

    if(dat->d.pm.gettree1 < 8)
        return (uint16_t)(pmarc2_hist_lookup(dat,
                                             pmarc2_historyBase[dat->d.pm.gettree1] +
                                                 getbits(dat, (uint8_t)pmarc2_historyBits[dat->d.pm.gettree1])));

    if(dat->d.pm.gettree1 < 23) return (uint16_t)(PMARC2_OFFSET + 2 + (dat->d.pm.gettree1 - 8));

    return (uint16_t)(PMARC2_OFFSET + pmarc2_repeatBase[dat->d.pm.gettree1 - 23] +
                      getbits(dat, (uint8_t)pmarc2_repeatBits[dat->d.pm.gettree1 - 23]));
}

static uint16_t decode_p_pm2(struct lhold_data *dat)
{
    int32_t nbits, delta, gettree2;

    if(dat->d.pm.gettree1 == 8)
    {
        nbits = 6;
        delta = 0;
    }
    else if(dat->d.pm.gettree1 < 28)
    {
        if(!(gettree2 = pmarc2_tree_get(dat, &dat->d.pm.tree2)))
        {
            nbits = 6;
            delta = 0;
        }
        else
        {
            nbits = 5 + gettree2;
            delta = 1 << nbits;
        }
    }
    else
    {
        nbits = 0;
        delta = 0;
    }

    return (uint16_t)(delta + getbits(dat, (uint8_t)nbits));
}

/* --- Main decompression dispatcher --- */

#define METHOD_LH2 0
#define METHOD_LH3 1
#define METHOD_PM2 2

static int lhold_decrunch(lhold_io *io, int method)
{
    struct lhold_data *dd;
    int                result = -1;

    dd = (struct lhold_data *)calloc(1, sizeof(struct lhold_data));

    if(!dd) return -1;

    dd->io     = io;
    dd->dicbit = 13;

    {
        void (*decodeStart)(struct lhold_data *) = NULL;
        uint16_t (*decodeC)(struct lhold_data *) = NULL;
        uint16_t (*decodeP)(struct lhold_data *) = NULL;

        switch(method)
        {
            case METHOD_LH2:
                decodeStart = decode_start_dyn;
                decodeC     = decode_c_dyn;
                decodeP     = decode_p_dyn;
                break;
            case METHOD_LH3:
                decodeStart = decode_start_st0;
                decodeP     = decode_p_st0;
                decodeC     = decode_c_st0;
                break;
            case METHOD_PM2:
                decodeStart = decode_start_pm2;
                decodeP     = decode_p_pm2;
                decodeC     = decode_c_pm2;
                break;
            default: free(dd); return -1;
        }

        {
            uint32_t dicsiz = (uint32_t)(1 << dd->dicbit);
            int32_t  offset = (method == METHOD_PM2) ? 0x100 - 2 : 0x100 - 3;

            dd->text = (uint8_t *)calloc(dicsiz, 1);

            if(!dd->text)
            {
                free(dd);
                return -1;
            }

            memset(dd->text, ' ', dicsiz);

            decodeStart(dd);
            dicsiz--; /* now used with AND */

            while(!lhold_at_end(io) && !io->error)
            {
                int32_t c = decodeC(dd);

                if(io->error) break;

                if(c <= UCHAR_MAX_VAL)
                {
                    dd->text[dd->loc++] = (uint8_t)c;
                    lhold_putchar(io, (uint8_t)c);
                    dd->loc &= dicsiz;
                    dd->count++;
                }
                else
                {
                    int32_t  i;
                    uint16_t p;

                    c -= offset;
                    p = decodeP(dd);

                    if(io->error) break;

                    i = (int32_t)(dd->loc - p - 1);
                    dd->count += (uint32_t)c;

                    while(c--)
                    {
                        uint8_t byte = dd->text[i++ & dicsiz];
                        dd->text[dd->loc++] = byte;
                        lhold_putchar(io, byte);
                        dd->loc &= dicsiz;
                    }
                }
            }

            result = io->error ? -1 : 0;
            free(dd->text);
        }
    }

    free(dd);
    return result;
}

/* --- Public API --- */

AARU_EXPORT int AARU_CALL lha_decompress_lh2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    lhold_io io;
    int      result;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    memset(&io, 0, sizeof(io));
    io.in_buf  = in_buf;
    io.in_len  = in_len;
    io.out_buf = out_buf;
    io.out_len = *out_len;

    result  = lhold_decrunch(&io, METHOD_LH2);
    *out_len = io.out_pos;
    return result;
}

AARU_EXPORT int AARU_CALL lha_decompress_lh3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    lhold_io io;
    int      result;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    memset(&io, 0, sizeof(io));
    io.in_buf  = in_buf;
    io.in_len  = in_len;
    io.out_buf = out_buf;
    io.out_len = *out_len;

    result  = lhold_decrunch(&io, METHOD_LH3);
    *out_len = io.out_pos;
    return result;
}

AARU_EXPORT int AARU_CALL pmarc_decompress_pm2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    lhold_io io;
    int      result;

    if(!in_buf || !out_buf || !out_len || in_len == 0) return -1;

    memset(&io, 0, sizeof(io));
    io.in_buf  = in_buf;
    io.in_len  = in_len;
    io.out_buf = out_buf;
    io.out_len = *out_len;

    result  = lhold_decrunch(&io, METHOD_PM2);
    *out_len = io.out_pos;
    return result;
}
