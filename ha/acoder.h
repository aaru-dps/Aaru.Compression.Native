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
  Modified to work with memory buffers instead of files by
  Copyright (C) 2005 Natalia Portillo
************************************************************************/

#ifndef ACODER_H
#define ACODER_H

#include "internal.h"

void ac_init_decode(decompress_context_t *ctx);

void ac_in(U16B low, U16B high, U16B tot);

U16B ac_threshold_val(U16B tot);

#endif /* ACODER_H */
