/*$Source: /usr/home/dhesi/zoo/RCS/huf.c,v $*/
/*$Id: huf.c,v 1.9 91/07/09 01:39:55 dhesi Exp $*/
/***********************************************************
    huf.c -- static Huffman

Adapted from "ar" archiver written by Haruhiko Okumura.
***********************************************************/
// Modified for in-memory decompression by Natalia Portillo, 2025

#include <limits.h>

#include "ar.h"
#include "lzh.h"

#define NP   (DICBIT + 1)
#define NT   (CODE_BIT + 3)
#define PBIT 4 /* smallest integer such that (1U << PBIT) > NP */
#define TBIT 5 /* smallest integer such that (1U << TBIT) > NT */
#if NT > NP
#define NPT NT
#else
#define NPT NP
#endif

static void read_pt_len(int, int, int);
static void read_c_len();

int decoded; /* for use in decode.c */

uint16_t        left[2 * NC - 1], right[2 * NC - 1];
static uint8_t *buf, c_len[NC], pt_len[NPT];
static uint32_t bufsiz = 0, blocksize;
static uint16_t c_freq[2 * NC - 1], c_table[4096], c_code[NC], p_freq[2 * NP - 1], pt_table[256], pt_code[NPT],
    t_freq[2 * NT - 1];

/***** decoding *****/

static void read_pt_len(int nn, int nbit, int i_special)
{
    int      i, c, n;
    uint32_t mask;

    n = getbits(nbit);
    if(n == 0)
    {
        c = getbits(nbit);
        for(i = 0; i < nn; i++) pt_len[i] = 0;
        for(i = 0; i < 256; i++) pt_table[i] = c;
    }
    else
    {
        i = 0;
        while(i < n)
        {
            c = bitbuf >> (BITBUFSIZ - 3);
            if(c == 7)
            {
                mask = (unsigned)1 << (BITBUFSIZ - 1 - 3);
                while(mask & bitbuf)
                {
                    mask >>= 1;
                    c++;
                }
            }
            fillbuf((c < 7) ? 3 : c - 3);
            pt_len[i++] = c;
            if(i == i_special)
            {
                c = getbits(2);
                while(--c >= 0) pt_len[i++] = 0;
            }
        }
        while(i < nn) pt_len[i++] = 0;
        make_table(nn, pt_len, 8, pt_table);
    }
}

static void read_c_len()
{
    int      i, c, n;
    uint32_t mask;

    n = getbits(CBIT);
    if(n == 0)
    {
        c = getbits(CBIT);
        for(i = 0; i < NC; i++) c_len[i] = 0;
        for(i = 0; i < 4096; i++) c_table[i] = c;
    }
    else
    {
        i = 0;
        while(i < n)
        {
            c = pt_table[bitbuf >> (BITBUFSIZ - 8)];
            if(c >= NT)
            {
                mask = (unsigned)1 << (BITBUFSIZ - 1 - 8);
                do {
                    if(bitbuf & mask)
                        c = right[c];
                    else
                        c = left[c];
                    mask >>= 1;
                } while(c >= NT);
            }
            fillbuf(pt_len[c]);
            if(c <= 2)
            {
                if(c == 0)
                    c = 1;
                else if(c == 1)
                    c = getbits(4) + 3;
                else
                    c = getbits(CBIT) + 20;
                while(--c >= 0) c_len[i++] = 0;
            }
            else
                c_len[i++] = c - 2;
        }
        while(i < NC) c_len[i++] = 0;
        make_table(NC, c_len, 12, c_table);
    }
}

uint32_t decode_c()
{
    uint32_t j, mask;

    if(blocksize == 0)
    {
        blocksize = getbits(16);
        if(blocksize == 0)
        {
#if 0
			(void) fprintf(stderr, "block size = 0, decoded\n");  /* debug */
#endif
            decoded = 1;
            return 0;
        }
        read_pt_len(NT, TBIT, 3);
        read_c_len();
        read_pt_len(NP, PBIT, -1);
    }
    blocksize--;
    j = c_table[bitbuf >> (BITBUFSIZ - 12)];
    if(j >= NC)
    {
        mask = (unsigned)1 << (BITBUFSIZ - 1 - 12);
        do {
            if(bitbuf & mask)
                j = right[j];
            else
                j = left[j];
            mask >>= 1;
        } while(j >= NC);
    }
    fillbuf(c_len[j]);
    return j;
}

uint32_t decode_p()
{
    uint32_t j, mask;

    j = pt_table[bitbuf >> (BITBUFSIZ - 8)];
    if(j >= NP)
    {
        mask = (unsigned)1 << (BITBUFSIZ - 1 - 8);
        do {
            if(bitbuf & mask)
                j = right[j];
            else
                j = left[j];
            mask >>= 1;
        } while(j >= NP);
    }
    fillbuf(pt_len[j]);
    if(j != 0) j = ((unsigned)1 << (j - 1)) + getbits((int)(j - 1));
    return j;
}

void huf_decode_start()
{
    init_getbits();
    blocksize = 0;
}
