/*$Source: /usr/home/dhesi/zoo/RCS/decode.c,v $*/
/*$Id: decode.c,v 1.6 91/07/09 01:39:49 dhesi Exp $*/
/***********************************************************
    decode.c

Adapted from "ar" archiver written by Haruhiko Okumura.
***********************************************************/
// Modified for in-memory decompression by Natalia Portillo, 2025

#include <limits.h>
#include <stdint.h>

#include "ar.h"
#include "lzh.h"

extern int decoded; /* from huf.c */

static int j; /* remaining bytes to copy */

void decode_start()
{
    huf_decode_start();
    j       = 0;
    decoded = 0;
}

/*
decodes; returns no. of chars decoded
*/

int decode(uint32_t count, uint8_t *buffer)
/* The calling function must keep the number of
   bytes to be processed.  This function decodes
   either 'count' bytes or 'DICSIZ' bytes, whichever
   is smaller, into the array 'buffer[]' of size
   'DICSIZ' or more.
   Call decode_start() once for each new file
   before calling this function. */
{
    static uint32_t i;
    uint32_t        r, c;

    r = 0;
    while(--j >= 0)
    {
        buffer[r] = buffer[i];
        i         = (i + 1) & (DICSIZ - 1);
        if(++r == count) return r;
    }
    for(;;)
    {
        c = decode_c();
        if(decoded) return r;
        if(c <= UCHAR_MAX)
        {
            buffer[r] = c;
            if(++r == count) return r;
        }
        else
        {
            j = c - (UCHAR_MAX + 1 - THRESHOLD);
            i = (r - decode_p() - 1) & (DICSIZ - 1);
            while(--j >= 0)
            {
                buffer[r] = buffer[i];
                i         = (i + 1) & (DICSIZ - 1);
                if(++r == count) return r;
            }
        }
    }
}
