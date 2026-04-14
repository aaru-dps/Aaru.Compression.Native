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

#include "bitio.h"

#include <string.h>

void lha_bitio_init(lha_bitio *self, const uint8_t *buffer, size_t length)
{
    self->buffer  = buffer;
    self->length  = length;
    self->pos     = 0;
    self->bits    = 0;
    self->numbits = 0;
    self->eof     = false;
}

static void lha_bitio_fill(lha_bitio *self)
{
    while(self->numbits <= 24 && self->pos < self->length)
    {
        self->bits |= (uint32_t)self->buffer[self->pos++] << (24 - self->numbits);
        self->numbits += 8;
    }

    if(self->pos >= self->length && self->numbits == 0) self->eof = true;
}

bool lha_bitio_at_eof(lha_bitio *self) { return self->eof && self->numbits == 0; }

unsigned int lha_bitio_next_bit(lha_bitio *self)
{
    if(self->numbits == 0) lha_bitio_fill(self);

    if(self->numbits == 0) return 0;

    unsigned int bit = (self->bits >> 31) & 1;
    self->bits <<= 1;
    self->numbits--;
    return bit;
}

unsigned int lha_bitio_next_bits(lha_bitio *self, int numbits)
{
    if(numbits == 0) return 0;

    if((int)self->numbits < numbits) lha_bitio_fill(self);

    unsigned int result = self->bits >> (32 - numbits);
    self->bits <<= numbits;
    self->numbits -= numbits;
    return result;
}

unsigned int lha_bitio_peek_bits(lha_bitio *self, int numbits)
{
    if(numbits == 0) return 0;

    if((int)self->numbits < numbits) lha_bitio_fill(self);

    return self->bits >> (32 - numbits);
}

void lha_bitio_skip_bits(lha_bitio *self, int numbits)
{
    self->bits <<= numbits;
    self->numbits -= numbits;
}

int lha_bitio_next_byte(lha_bitio *self)
{
    if(self->pos >= self->length && self->numbits < 8) return -1;

    /* If on byte boundary, read directly */
    if(self->numbits == 0)
    {
        if(self->pos < self->length) return self->buffer[self->pos++];

        return -1;
    }

    return (int)lha_bitio_next_bits(self, 8);
}
