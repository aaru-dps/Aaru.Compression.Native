/*$Source: /usr/home/dhesi/zoo/RCS/maketbl.c,v $*/
/*$Id: maketbl.c,v 1.8 91/07/09 01:39:52 dhesi Exp $*/
/***********************************************************
    maketbl.c -- make table for decoding

    Builds a fast lookup table + fallback tree for Huffman
    codes given code lengths.  Used by decode_c() to map
    input bit patterns to symbols efficiently.

    Adapted from Haruhiko Okumura’s “ar” archiver.
    Modified for in-memory decompression by Natalia Portillo, 2025
***********************************************************/

#include <stdio.h>
#include "ar.h"   // provides NC, CODE_BIT, etc.
#include "lzh.h"  // provides BITBUFSIZ

/*
 * make_table(nchar, bitlen, tablebits, table):
 *
 * nchar     = number of symbols
 * bitlen[]  = array of code lengths for each symbol [0..nchar-1]
 * tablebits = number of bits for fast direct lookup
 * table[]   = output table of size (1<<tablebits), entries are:
 *             - symbol index if code length ≤ tablebits
 *             - zero or tree node index to follow for longer codes
 *
 * Algorithm steps:
 *  1) Count how many codes of each length (count[1..16]).
 *  2) Compute 'start' offsets for each length in a 16-bit code space.
 *  3) Normalize starts to 'tablebits' prefix domain, build 'weight'.
 *  4) Fill direct-mapped entries for short codes.
 *  5) Build binary tree (using left[]/right[]) for codes longer than tablebits.
 */
void make_table(int nchar, uint8_t *bitlen, int tablebits, uint16_t *table)
{
    uint16_t  count[17];   // count[L] = number of symbols with length L
    uint16_t  weight[17];  // weight[L] = step size in prefix domain for length L
    uint16_t  start[18];   // start[L] = base code for length L in 16-bit space
    uint16_t *p;           // pointer into 'table' or tree
    uint32_t  i, k, len, ch;
    uint32_t  jutbits;   // bits to drop when mapping into tablebits
    uint32_t  avail;     // next free node index for left[]/right[] tree
    uint32_t  nextcode;  // end-of-range code for current length
    uint32_t  mask;      // bitmask for tree insertion

    // 1) Zero counts, then tally code-lengths
    for(i = 1; i <= 16; i++) count[i] = 0;
    for(i = 0; i < (uint32_t)nchar; i++) count[bitlen[i]]++;

    // 2) Compute cumulative start positions in the 16-bit code space
    start[1] = 0;
    for(i = 1; i <= 16; i++) start[i + 1] = start[i] + (count[i] << (16 - i));

    // Validate: sum of all codes must fill 16-bit range
    if(start[17] != (uint16_t)(1U << 16)) fprintf(stderr, "make_table: Bad decode table\n");

    // Prepare for mapping into tablebits-bit table
    jutbits = 16 - tablebits;
    for(i = 1; i <= (uint32_t)tablebits; i++)
    {
        // Shrink start[i] into prefix domain
        start[i] >>= jutbits;
        // Weight = 2^(tablebits - i)
        weight[i] = (uint16_t)(1U << (tablebits - i));
    }
    // For lengths > tablebits, weight = 2^(16 - length)
    for(; i <= 16; i++) weight[i] = (uint16_t)(1U << (16 - i));

    // 3) Clear any unused table slots between last short code and end
    i = start[tablebits + 1] >> jutbits;
    if(i != (uint16_t)(1U << tablebits))
    {
        k = 1U << tablebits;
        while(i < k) table[i++] = 0;
    }

    // Initialize tree node index after the direct table entries
    avail = nchar;
    // Mask for inspecting bits when building tree
    mask  = 1U << (15 - tablebits);

    // 4) For each symbol, place its codes in table or tree
    for(ch = 0; ch < (uint32_t)nchar; ch++)
    {
        len = bitlen[ch];
        if(len == 0) continue;  // skip symbols with no code

        // Next code range = [start[len], start[len]+weight[len])
        nextcode = start[len] + weight[len];

        if(len <= tablebits)
        {
            // Direct mapping: fill all table slots in this range
            for(k = start[len]; k < nextcode; k++) table[k] = (uint16_t)ch;
        }
        else
        {
            // Build or extend tree for longer codes
            // Start at table index for this prefix
            k              = start[len];
            p              = &table[k >> jutbits];
            // Number of extra bits beyond tablebits
            uint32_t extra = len - tablebits;

            // Walk/construct tree nodes bit by bit
            while(extra-- > 0)
            {
                if(*p == 0)
                {
                    // allocate a new node for left[]/right[]
                    left[avail] = right[avail] = 0;
                    *p                         = (uint16_t)avail++;
                }
                // branch left or right based on current code bit
                if(k & mask)
                    p = &right[*p];
                else
                    p = &left[*p];

                // shift to next bit in code
                k <<= 1;
            }
            // At leaf: assign symbol
            *p = (uint16_t)ch;
        }
        // Advance start[len] for next code of same length
        start[len] = nextcode;
    }
}
