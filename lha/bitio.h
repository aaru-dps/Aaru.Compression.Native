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

#ifndef AARU_COMPRESSION_NATIVE__LHA_BITIO_H_
#define AARU_COMPRESSION_NATIVE__LHA_BITIO_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct lha_bitio
{
    const uint8_t *buffer;
    size_t         length;
    size_t         pos;
    uint32_t       bits;
    unsigned int   numbits;
    bool           eof;
} lha_bitio;

void         lha_bitio_init(lha_bitio *self, const uint8_t *buffer, size_t length);
bool         lha_bitio_at_eof(lha_bitio *self);
unsigned int lha_bitio_next_bit(lha_bitio *self);
unsigned int lha_bitio_next_bits(lha_bitio *self, int numbits);
unsigned int lha_bitio_peek_bits(lha_bitio *self, int numbits);
void         lha_bitio_skip_bits(lha_bitio *self, int numbits);
int          lha_bitio_next_byte(lha_bitio *self);

#endif /* AARU_COMPRESSION_NATIVE__LHA_BITIO_H_ */
