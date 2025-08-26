/*$Source: /usr/home/dhesi/zoo/RCS/io.c,v $*/
/*$Id: io.c,v 1.14 91/07/09 01:39:54 dhesi Exp $*/
/***********************************************************
    io.c -- input/output (modified for in-memory I/O)

    Adapted from Haruhiko Okumura’s “ar” archiver.
    This version feeds compressed bytes from a memory buffer
    (via mem_getc()) and writes decompressed output to a buffer
    (via mem_putc()), eliminating FILE* dependencies.
    Modified for in-memory decompression by Natalia Portillo, 2025
***********************************************************/

#include <limits.h>  // Provides CHAR_BIT for bit-width operations

#include "ar.h"   // Archive format constants (e.g., CODE_BIT, NC)
#include "lh5.h"  // Declarations for mem_getc(), mem_putc(), buffer state
#include "lzh.h"  // LZH algorithm constants (e.g., BITBUFSIZ, DICSIZ)

//-----------------------------------------------------------------------------
// Global bit-I/O state
//-----------------------------------------------------------------------------

uint16_t bitbuf;      // Accumulates bits shifted in from the input stream
int      unpackable;  // Unused in decompression here (was for encode error)
// Byte counters (optional diagnostics; not used to gate decompression)
size_t   compsize;   // Count of output bytes produced (for compression mode)
size_t   origsize;   // Count of input bytes consumed (for CRC in file I/O)
uint32_t subbitbuf;  // Holds the last byte fetched; bits are consumed from here
int      bitcount;   // How many valid bits remain in subbitbuf

//-----------------------------------------------------------------------------
// fillbuf(n)
//   Shift the global bitbuf left by n bits, then read in n new bits
//   from the input buffer (in-memory) to replenish bitbuf.
//-----------------------------------------------------------------------------
void fillbuf(int n) /* Shift bitbuf n bits left, read n bits */
{
    // Make room for n bits
    bitbuf <<= n;

    // While we still need more bits than we have in subbitbuf...
    while(n > bitcount)
    {
        // Pull any remaining bits from subbitbuf into bitbuf
        bitbuf |= subbitbuf << (n -= bitcount);

        // Fetch the next compressed byte from input memory
        {
            int c     = mem_getc();  // read one byte or 0 at EOF
            subbitbuf = (c == EOF ? 0 : (uint8_t)c);
        }

        // Reset bitcount: a full new byte is available
        bitcount = CHAR_BIT;
    }

    // Finally, consume the last n bits from subbitbuf into bitbuf
    bitbuf |= subbitbuf >> (bitcount -= n);
}

//-----------------------------------------------------------------------------
// getbits(n)
//   Return the next n bits from bitbuf (highest-order bits), then
//   call fillbuf(n) to replace them. Useful for reading variable-length codes.
//-----------------------------------------------------------------------------
uint32_t getbits(int n)
{
    uint32_t x = bitbuf >> (BITBUFSIZ - n);  // extract top n bits
    fillbuf(n);                              // replenish bitbuf for future reads
    return x;
}

//-----------------------------------------------------------------------------
// putbits(n, x)
//   Write the lowest n bits of x into the output buffer, packing them
//   into bytes via subbitbuf/bitcount and sending full bytes out
//   with mem_putc(). Used by the encoder; kept here for completeness.
//-----------------------------------------------------------------------------
void putbits(int n, uint32_t x) /* Write rightmost n bits of x */
{
    // If we have enough room in subbitbuf, just pack the bits
    if(n < bitcount) { subbitbuf |= x << (bitcount -= n); }
    else
    {
        // Output the first full byte when subbitbuf fills
        {
            int w = (int)(subbitbuf | (x >> (n -= bitcount)));
            mem_putc(w);
            compsize++;  // increment output counter (for compression)
        }

        // If remaining bits don't fill a full byte, stash them
        if(n < CHAR_BIT) { subbitbuf = x << (bitcount = CHAR_BIT - n); }
        else
        {
            // Otherwise, flush a second full byte
            {
                int w2 = (int)(x >> (n - CHAR_BIT));
                mem_putc(w2);
                compsize++;
            }
            // And stash any leftover bits beyond two bytes
            subbitbuf = x << (bitcount = 2 * CHAR_BIT - n);
        }
    }
}

//-----------------------------------------------------------------------------
// init_getbits()
//   Reset the bit-reader state so that fillbuf() will load fresh bits
//   from the start of the input buffer.
//-----------------------------------------------------------------------------
void init_getbits()
{
    bitbuf    = 0;       // clear accumulated bits
    subbitbuf = 0;       // no pending byte
    bitcount  = 0;       // no bits available
    fillbuf(BITBUFSIZ);  // pre-load the bit buffer fully
}

//-----------------------------------------------------------------------------
// init_putbits()
//   Reset the bit-writer state so subsequent putbits() calls start fresh.
//-----------------------------------------------------------------------------
void init_putbits()
{
    bitcount  = CHAR_BIT;  // subbitbuf is empty but ready for CHAR_BIT bits
    subbitbuf = 0;         // clear any leftover byte data
}
