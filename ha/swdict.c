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
    HA sliding window dictionary
***********************************************************************/

/***********************************************************************
  Modified to work with memory buffers instead of files by
  Copyright (C) 2005 Natalia Portillo
************************************************************************/

#include "internal.h"
#include "swdict.h"
#include <stdlib.h>
#include <string.h>

U16B swd_bpos, swd_mlf;
S16B swd_char;

static decompress_context_t *g_ctx = NULL;
static U16B                  cblen, bbf;
static unsigned char *       b = NULL;

void swd_cleanup(void)
{
    if(b != NULL)
    {
        free(b);
        b = NULL;
    }
}

void swd_dinit(decompress_context_t *ctx, U16B bufl)
{
    g_ctx = ctx;
    cblen = bufl;

    b = malloc(cblen * sizeof(unsigned char));
    if(b == NULL)
    {
        ctx->output.error = 1;
        return;
    }

    bbf = 0;
}

static int putbyte_to_buffer(unsigned char c)
{
    if(!g_ctx || g_ctx->output.pos >= g_ctx->output.max_size)
    {
        if(g_ctx) g_ctx->output.error = 1;
        return -1;
    }

    g_ctx->output.data[g_ctx->output.pos++] = c;
    if(g_ctx->output.pos > g_ctx->output.size) { g_ctx->output.size = g_ctx->output.pos; }
    return 0;
}

void swd_dpair(U16B l, U16B p)
{
    if(!g_ctx || g_ctx->output.error) return;

    if(bbf > p) p = bbf - 1 - p;
    else p        = cblen - 1 - p + bbf;

    while(l--)
    {
        b[bbf] = b[p];
        if(putbyte_to_buffer(b[p]) < 0) return;
        if(++bbf == cblen) bbf = 0;
        if(++p == cblen) p = 0;
    }
}

void swd_dchar(S16B c)
{
    if(!g_ctx || g_ctx->output.error) return;

    b[bbf] = c;
    if(putbyte_to_buffer(c) < 0) return;
    if(++bbf == cblen) bbf = 0;
}
