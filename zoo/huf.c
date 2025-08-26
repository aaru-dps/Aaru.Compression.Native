/*$Source: /usr/home/dhesi/zoo/RCS/huf.c,v $*/
/*$Id: huf.c,v 1.9 91/07/09 01:39:55 dhesi Exp $*/
/***********************************************************
    huf.c -- static Huffman decoding

  Adapted from Haruhiko Okumura’s “ar” archiver.
  Modified in 2025 by Natalia Portillo for in-memory I/O.
***********************************************************/

#include <limits.h>  // UCHAR_MAX
#include "ar.h"      // archive format constants
#include "lzh.h"     // LZH algorithm constants (NC, DICBIT, CODE_BIT, etc.)

// NP = number of position codes = DICBIT+1
// NT = number of tree codes   = CODE_BIT+3
// PBIT, TBIT = bit‐width to transmit NP/NT in header
#define NP   (DICBIT + 1)
#define NT   (CODE_BIT + 3)
#define PBIT 4 /* smallest bits so (1<<PBIT)>NP */
#define TBIT 5 /* smallest bits so (1<<TBIT)>NT */

// NPT = max(NP,NT) for prefix‐tree lengths
#if NT > NP
#define NPT NT
#else
#define NPT NP
#endif

// forward declarations of helper routines
static void read_pt_len(int nn, int nbit, int i_special);
static void read_c_len(void);

int decoded;  // flag set when end-of-stream block is seen

// Huffman tree storage arrays
// left[]/right[] store the binary tree structure for fast decoding
uint16_t left[2 * NC - 1], right[2 * NC - 1];

// c_len[] = code lengths for literal/length tree (NC symbols)
// pt_len[] = code lengths for position‐tree / prefix table (NPT symbols)
// buf     = temporary buffer pointer used during encoding; unused in decode
static uint8_t *buf, c_len[NC], pt_len[NPT];

// size of buf if used, and remaining symbols in current block
static uint32_t bufsiz = 0, blocksize;

// Frequency, code and decode‐table structures
static uint16_t c_freq[2 * NC - 1],  // literal/length frequency counts
    c_table[4096],                   // fast‐lookup table for literal/length decoding
    c_code[NC],                      // canonical Huffman codes for literals
    p_freq[2 * NP - 1],              // position frequency counts
    pt_table[256],                   // prefix‐tree fast lookup (for reading code lengths)
    pt_code[NPT],                    // canonical codes for prefix‐tree
    t_freq[2 * NT - 1];              // temporary freq for tree of code‐length codes

/***** decoding helper: read prefix‐tree code-lengths *****/
static void read_pt_len(int nn, int nbit, int i_special)
{
    int      i, c, n;
    uint32_t mask;

    // 1) read how many code‐lengths to consume
    n = getbits(nbit);
    if(n == 0)
    {
        // special case: all code‐lengths are identical
        c = getbits(nbit);
        for(i = 0; i < nn; i++)  // zero out lengths
            pt_len[i] = 0;
        for(i = 0; i < 256; i++)  // prefix‐table always returns 'c'
            pt_table[i] = c;
    }
    else
    {
        // 2) read code lengths one by one
        i = 0;
        while(i < n)
        {
            // peek top 3 bits of bitbuf to guess small lengths
            c = bitbuf >> (BITBUFSIZ - 3);
            if(c == 7)
            {
                // if all three bits are 1, count additional ones
                mask = 1U << (BITBUFSIZ - 1 - 3);
                while(mask & bitbuf)
                {
                    c++;
                    mask >>= 1;
                }
            }
            // consume the actual length bits
            fillbuf((c < 7) ? 3 : (c - 3));
            pt_len[i++] = c;

            // at special index, read a small run of zeros
            if(i == i_special)
            {
                c = getbits(2);
                while(--c >= 0 && i < nn) pt_len[i++] = 0;
            }
        }
        // any remaining symbols get code‐length zero
        while(i < nn) pt_len[i++] = 0;

        // build fast lookup table from lengths
        make_table(nn, pt_len, 8, pt_table);
    }
}

/***** decoding helper: read literal/length code‐lengths *****/
static void read_c_len(void)
{
    int      i, c, n;
    uint32_t mask;

    // 1) how many literal codes?
    n = getbits(CBIT);
    if(n == 0)
    {
        // all code‐lengths identical
        c = getbits(CBIT);
        for(i = 0; i < NC; i++) c_len[i] = 0;
        for(i = 0; i < 4096; i++) c_table[i] = c;
    }
    else
    {
        // 2) read each code length via prefix‐tree
        i = 0;
        while(i < n)
        {
            // lookup next symbol in prefix‐table
            c = pt_table[bitbuf >> (BITBUFSIZ - 8)];
            if(c >= NT)
            {
                // if prefix code is non-leaf, walk tree
                mask = 1U << (BITBUFSIZ - 1 - 8);
                do {
                    c = (bitbuf & mask) ? right[c] : left[c];
                    mask >>= 1;
                } while(c >= NT);
            }
            // consume code‐length bits
            fillbuf(pt_len[c]);

            // c ≤ 2: run-length encoding of zeros
            if(c <= 2)
            {
                if(c == 0)
                    c = 1;
                else if(c == 1)
                    c = getbits(4) + 3;
                else
                    c = getbits(CBIT) + 20;
                while(--c >= 0 && i < NC) c_len[i++] = 0;
            }
            else
            {
                // real code-length = c−2
                c_len[i++] = (uint8_t)(c - 2);
            }
        }
        // fill rest with zero lengths
        while(i < NC) c_len[i++] = 0;

        // build fast lookup for literal/length codes
        make_table(NC, c_len, 12, c_table);
    }
}

/***** decode next literal/length symbol or end-of-block *****/
uint32_t decode_c(void)
{
    uint32_t j, mask;

    // if starting a new block, read its header
    if(blocksize == 0)
    {
        blocksize = getbits(16);  // block size = number of symbols
        if(blocksize == 0)
        {  // zero block → end of data
            decoded = 1;
            return 0;
        }
        // read three Huffman trees for this block:
        //   1) code-length codes for literal tree  (NT,TBIT,3)
        read_pt_len(NT, TBIT, 3);
        //   2) literal/length tree lengths         (CBIT)
        read_c_len();
        //   3) prefix-tree lengths for positions    (NP,PBIT,-1)
        read_pt_len(NP, PBIT, -1);
    }

    // consume one symbol from this block
    blocksize--;

    // fast table lookup: top 12 bits
    j = c_table[bitbuf >> (BITBUFSIZ - 12)];
    if(j >= NC)
    {
        // need to walk tree if overflow
        mask = 1U << (BITBUFSIZ - 1 - 12);
        do {
            j = (bitbuf & mask) ? right[j] : left[j];
            mask >>= 1;
        } while(j >= NC);
    }

    // remove j’s code length bits from bitbuf
    fillbuf(c_len[j]);
    return j;
}

/***** decode match-position extra bits *****/
uint32_t decode_p(void)
{
    uint32_t j, mask;

    // fast table lookup: top 8 bits
    j = pt_table[bitbuf >> (BITBUFSIZ - 8)];
    if(j >= NP)
    {
        // tree walk for long codes
        mask = 1U << (BITBUFSIZ - 1 - 8);
        do {
            j = (bitbuf & mask) ? right[j] : left[j];
            mask >>= 1;
        } while(j >= NP);
    }

    // consume prefix bits
    fillbuf(pt_len[j]);

    // if non-zero, read extra bits to form full position
    if(j != 0) j = (1U << (j - 1)) + getbits((int)(j - 1));

    return j;
}

/***** start a new Huffman decode session *****/
void huf_decode_start(void)
{
    init_getbits();  // reset bit buffer & subbitbuf state
    blocksize = 0;   // force reading a fresh block header
}
