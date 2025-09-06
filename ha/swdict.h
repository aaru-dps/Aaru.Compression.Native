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

#ifndef SWDICT_H
#define SWDICT_H

#include "internal.h"

#define MINLEN 3    /* Minimum possible match length */

void swd_dinit(decompress_context_t *ctx, U16B bufl);

void swd_cleanup(void);

void swd_dpair(U16B l, U16B p);

void swd_dchar(S16B c);

extern U16B swd_bpos, swd_mlf;
extern S16B swd_char;

#endif /* SWDICT_H */
