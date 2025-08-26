/* $Id: lzh.c,v 1.15 91/07/06 19:18:51 dhesi Exp $ */
// Modified for in-memory decompression by Natalia Portillo, 2025

#include "lzh.h" /* prototypes for encode(), lzh_decode() */
#include <stdint.h>
#include "ar.h"
#include "lh5.h" /* mem_getc(), mem_putc(), in_ptr/in_left, out_ptr/out_left */

extern int decoded; /* from huf.c */

/*
 * lzh_decode now reads from in_buf/in_len and writes into out_buf/out_len
 * entirely in memory, via mem_getc()/mem_putc().
 */
int lzh_decode(void)
{
    int     n, i;
    uint8_t buffer[DICSIZ];

    /* Initialize the Huffman bit reader and sliding-window state */
    decode_start();

    /* Decode blocks of up to DICSIZ bytes until end‐of‐stream */
    while(!decoded)
    {
        n = decode(DICSIZ, buffer);
        for(i = 0; i < n; i++) { mem_putc(buffer[i]); }
    }

    return mem_error ? -1 : 0;
}
