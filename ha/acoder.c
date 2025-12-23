/***********************************************************************
  This file is part of HA, a general purpose file archiver.
  Copyright (C) 1995 Harri Hirvola

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
************************************************************************
    HA arithmetic coder
***********************************************************************/

/***********************************************************************
This file contains some small changes made by Nico de Vries (AIP-NL)
allowing it to be compiled with Borland C++ 3.1.
***********************************************************************/

/***********************************************************************
  Modified to work with memory buffers instead of files by
  Copyright (C) 2025-2026 Natalia Portillo
************************************************************************/

#include "internal.h"
#include "acoder.h"
#include <string.h>

static decompress_context_t *g_ctx = NULL;
static U16B                  h, l, v;
static S16B                  gpat;

/* Get byte from input buffer */
static int getbyte_from_buffer(void)
{
    if(!g_ctx || g_ctx->input.pos >= g_ctx->input.size) { return -1; }
    return g_ctx->input.data[g_ctx->input.pos++];
}

/* Bit input from memory buffer */
static int getbit_from_buffer(void)
{
    if(!g_ctx || g_ctx->input.error) { return -1; }

    gpat <<= 1;
    if(!(gpat & 0xff))
    {
        int byte = getbyte_from_buffer();
        if(byte < 0) { gpat = 0x100; }
        else
        {
            gpat = byte;
            gpat <<= 1;
            gpat |= 1;
        }
    }
    return (gpat & 0x100) >> 8;
}

#define getbit(b) { \
    int _bit = getbit_from_buffer(); \
    if (_bit < 0) { \
        g_ctx->input.error = 1; \
        b = 0; \
    } else { \
        b |= _bit; \
    } \
}

void ac_init_decode(decompress_context_t *ctx)
{
    g_ctx = ctx;
    h     = 0xffff;
    l     = 0;
    gpat  = 0;

    int b1 = getbyte_from_buffer();
    int b2 = getbyte_from_buffer();

    if(b1 < 0 || b2 < 0)
    {
        ctx->input.error = 1;
        v                = 0;
        return;
    }

    v = (b1 << 8) | (0xff & b2);
}

void ac_in(U16B low, U16B high, U16B tot)
{
    if(!g_ctx || g_ctx->input.error) return;

    U32B r = (U32B)(h - l) + 1;
    h      = (U16B)(r * high / tot - 1) + l;
    l += (U16B)(r * low / tot);

    while(!((h ^ l) & 0x8000))
    {
        l <<= 1;
        h <<= 1;
        h |= 1;
        v <<= 1;
        getbit(v);
        if(g_ctx->input.error) return;
    }

    while((l & 0x4000) && !(h & 0x4000))
    {
        l <<= 1;
        l &= 0x7fff;
        h <<= 1;
        h |= 0x8001;
        v <<= 1;
        v ^= 0x8000;
        getbit(v);
        if(g_ctx->input.error) return;
    }
}

U16B ac_threshold_val(U16B tot)
{
    if(!g_ctx || g_ctx->input.error) return 0;

    U32B r = (U32B)(h - l) + 1;
    return (U16B)((((U32B)(v - l) + 1) * tot - 1) / r);
}
