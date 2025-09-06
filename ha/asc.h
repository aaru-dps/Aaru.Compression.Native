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
    HA ASC method
***********************************************************************/

/***********************************************************************
  Modified to work with memory buffers instead of files by
  Copyright (C) 2005 Natalia Portillo
************************************************************************/

#ifndef ASC_H
#define ASC_H

#include "internal.h"

int asc_unpack(decompress_context_t *ctx);

#define POSCODES 31200
#define SLCODES 16
#define LLCODES 48
#define LLLEN 16
#define LLBITS 4
#define LLMASK (LLLEN-1)
#define LENCODES (SLCODES+LLCODES*LLLEN)
#define LTCODES (SLCODES+LLCODES)
#define CTCODES 256
#define PTCODES 16
#define LTSTEP 8
#define MAXLT (750*LTSTEP)
#define CTSTEP 1
#define MAXCT (1000*CTSTEP)
#define PTSTEP 24
#define MAXPT (250*PTSTEP)
#define TTSTEP 40
#define MAXTT (150*TTSTEP)
#define TTORD 4
#define TTOMASK (TTORD-1)
#define LCUTOFF (3*LTSTEP)
#define CCUTOFF (3*CTSTEP)
#define CPLEN 8
#define LPLEN 4
#define MINLENLIM 4096

static U16B ltab[2 * LTCODES];
static U16B eltab[2 * LTCODES];
static U16B ptab[2 * PTCODES];
static U16B ctab[2 * CTCODES];
static U16B ectab[2 * CTCODES];
static U16B ttab[TTORD][2];
static U16B ccnt, pmax, npt;
static U16B ces;
static U16B les;
static U16B ttcon;

#endif /* ASC_H */
