/*$Source: /usr/home/dhesi/zoo/RCS/io.c,v $*/
/*$Id: io.c,v 1.14 91/07/09 01:39:54 dhesi Exp $*/
/***********************************************************
    io.c -- input/output (modified for in-memory I/O)

Adapted from "ar" archiver written by Haruhiko Okumura.
This version reads compressed bytes from an input buffer
via mem_getc() and writes output bytes to a buffer via
mem_putc(), removing all FILE* dependencies for decompression.
***********************************************************/
// Modified for in-memory decompression by Natalia Portillo, 2025

#include <limits.h>

#include "ar.h"
#include "lzh.h"

#include "lh5.h" /* mem_getc(), mem_putc(), in_ptr/in_left, out_ptr/out_left */

uint16_t bitbuf;
int      unpackable;
size_t   compsize, origsize;
uint32_t subbitbuf;
int      bitcount;

/*
 * fillbuf(n) -- shift bitbuf left by n bits and read in n new bits
 * now reads bytes directly from in-memory input buffer
 */
void fillbuf(int n) /* Shift bitbuf n bits left, read n bits */
{
    bitbuf <<= n;
    while(n > bitcount)
    {
        bitbuf |= subbitbuf << (n -= bitcount);

        /* fetch next compressed byte from in_buf */
        {
            int c     = mem_getc();
            subbitbuf = (c == EOF ? 0 : (uint8_t)c);
        }

        bitcount = CHAR_BIT;
    }
    bitbuf |= subbitbuf >> (bitcount -= n);
}

/*
 * getbits(n) -- return next n bits from the bit buffer
 */
uint32_t getbits(int n)
{
    uint32_t x = bitbuf >> (BITBUFSIZ - n);
    fillbuf(n);
    return x;
}

/*
 * putbits(n,x) -- write the lowest n bits of x to the bit buffer
 * now writes bytes directly to in-memory output buffer
 */
void putbits(int n, uint32_t x) /* Write rightmost n bits of x */
{
    if(n < bitcount) { subbitbuf |= x << (bitcount -= n); }
    else
    {
        /* output first byte */
        {
            int w = (int)(subbitbuf | (x >> (n -= bitcount)));
            mem_putc(w);
            compsize++;
        }
        if(n < CHAR_BIT) { subbitbuf = x << (bitcount = CHAR_BIT - n); }
        else
        {
            /* output second byte */
            {
                int w2 = (int)(x >> (n - CHAR_BIT));
                mem_putc(w2);
                compsize++;
            }
            subbitbuf = x << (bitcount = 2 * CHAR_BIT - n);
        }
    }
}

/*
 * init_getbits -- initialize bit reader state
 */
void init_getbits()
{
    bitbuf    = 0;
    subbitbuf = 0;
    bitcount  = 0;
    fillbuf(BITBUFSIZ);
}

/*
 * init_putbits -- initialize bit writer state
 */
void init_putbits()
{
    bitcount  = CHAR_BIT;
    subbitbuf = 0;
}
