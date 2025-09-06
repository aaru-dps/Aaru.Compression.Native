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
    HA HSC method
***********************************************************************/

/***********************************************************************
  Modified to work with memory buffers instead of files by
  Copyright (C) 2005 Natalia Portillo
************************************************************************/

#ifndef HSC_H
#define HSC_H

#include "internal.h"

int hsc_unpack(decompress_context_t *ctx);

#define IECLIM 32
#define NECLIM 5
#define NECTLIM 4
#define NECMAX 10
#define MAXCLEN 4
#define NUMCON 10000
#define NUMCFB 32760
#define ESCTH 3
#define MAXTVAL 8000
#define RFMINI 4
#define HTLEN 16384
#define NIL 0xffff
#define ESC 256

typedef unsigned char Context[4];

/* Model data */
static Context        curcon;
static U16B *         ht  = NULL;
static U16B *         hp  = NULL;
static Context *      con = NULL;
static unsigned char *cl  = NULL;
static unsigned char *cc  = NULL;
static U16B *         ft  = NULL;
static unsigned char *fe  = NULL;
static U16B *         elp = NULL;
static U16B *         eln = NULL;
static U16B           elf, ell;
static unsigned char *rfm = NULL;
static U16B *         fa  = NULL;
static unsigned char *fc  = NULL;
static U16B *         nb  = NULL;
static U16B           fcfbl;
static U16B           nrel;

/* Frequency mask system */
static unsigned char cmask[256];
static unsigned char cmstack[256];
static S16B          cmsp;

/* Escape probability modifying system variables */
static unsigned char nec;
static unsigned char iec[MAXCLEN + 1];

/* Update stack variables */
static U16B usp;
static U16B cps[MAXCLEN + 1];
static U16B as[MAXCLEN + 1];

/* Miscellaneous */
static S16B          dropcnt;
static unsigned char maxclen;
static U16B          hrt[HTLEN];
static U16B          hs[MAXCLEN + 1];
static S16B          cslen;

#endif /* HSC_H */
