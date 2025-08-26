/*$Source: /usr/home/dhesi/zoo/RCS/decode.c,v $*/
/*$Id: decode.c,v 1.6 91/07/09 01:39:49 dhesi Exp $*/
/***********************************************************
    decode.c

    Adapted from Haruhiko Okumura’s “ar” archiver. This
    version has been modified in 2025 by Natalia Portillo
    for in-memory decompression.
***********************************************************/

#include <limits.h>  // for UCHAR_MAX
#include <stdint.h>  // for fixed-width integer types

#include "ar.h"   // archive format constants
#include "lzh.h"  // LZH-specific constants (DICSIZ, THRESHOLD, etc.)

extern int decoded;  // flag set by decode_c() when end-of-stream is reached

static int j;  // number of literal/copy runs remaining from a match

/*
 * decode_start()
 *
 * Prepare the decoder for a new file:
 * - Initialize the Huffman bitstream (via huf_decode_start())
 * - Reset the sliding-window copy counter `j`
 * - Clear the end-of-data flag `decoded`
 */
void decode_start()
{
    huf_decode_start();  // reset bit-reader state
    j       = 0;         // no pending copy runs yet
    decoded = 0;         // not yet at end-of-stream
}

/*
 * decode(count, buffer)
 *
 * Decode up to `count` bytes (usually DICSIZ) into `buffer[]`.
 * Returns the actual number of bytes written, or 0 if `decoded` is set.
 *
 * Sliding‐window logic:
 * 1. If `j` > 0, we are in the middle of copying a previous match:
 *    - Copy one byte from `buffer[i]` into `buffer[r]`
 *    - Advance `i` (circular within DICSIZ) and `r`
 *    - Decrement `j` and repeat until `j` = 0 or `r` = count
 * 2. Otherwise, fetch the next symbol `c = decode_c()`:
 *    - If `c <= UCHAR_MAX`, it’s a literal byte: emit it directly
 *    - Else it’s a match:
 *        • compute `j = match_length = c - (UCHAR_MAX + 1 - THRESHOLD)`
 *        • compute `i = (r - match_offset - 1) mod DICSIZ`,
 *          where match_offset = decode_p()
 *        • enter copy loop from step 1
 */
int decode(uint32_t count, uint8_t *buffer)
{
    static uint32_t i;  // sliding-window read index (circular)
    uint32_t        r;  // write position in buffer
    uint32_t        c;  // symbol or match code

    r = 0;

    // Step 1: finish any pending copy from a previous match
    while(--j >= 0)
    {
        buffer[r] = buffer[i];               // copy one byte from history
        i         = (i + 1) & (DICSIZ - 1);  // wrap index within [0, DICSIZ)
        if(++r == count)                     // if output buffer is full
            return r;                        // return bytes written so far
    }

    // Step 2: decode new symbols until end-of-stream or buffer full
    for(;;)
    {
        c = decode_c();  // get next Huffman symbol
        if(decoded)      // end-of-stream marker reached
            return r;    // no more bytes to decode

        if(c <= UCHAR_MAX)
        {
            // Literal byte: emit it directly
            buffer[r] = (uint8_t)c;
            if(++r == count) return r;
        }
        else
        {
            // Match sequence: compute how many bytes to copy
            // j = match length
            j = c - (UCHAR_MAX + 1 - THRESHOLD);

            // i = start position in sliding window:
            //    current output position minus offset minus 1, wrapped
            i = (r - decode_p() - 1) & (DICSIZ - 1);

            // Copy `j` bytes from history
            while(--j >= 0)
            {
                buffer[r] = buffer[i];
                i         = (i + 1) & (DICSIZ - 1);
                if(++r == count) return r;
            }
        }
    }
}