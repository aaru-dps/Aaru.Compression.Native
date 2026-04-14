/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2026 Natalia Portillo.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "lzss.h"

#include <stdlib.h>
#include <string.h>

bool lha_lzss_init(lha_lzss *self, size_t window_size, uint8_t *out_buf, size_t out_size)
{
    self->window = (uint8_t *)malloc(window_size);

    if(!self->window) return false;

    self->mask     = window_size - 1; /* Assume power-of-two */
    self->position = 0;
    self->out_buf  = out_buf;
    self->out_pos  = 0;
    self->out_size = out_size;

    memset(self->window, 0, window_size);

    return true;
}

void lha_lzss_cleanup(lha_lzss *self)
{
    if(self->window)
    {
        free(self->window);
        self->window = NULL;
    }
}
