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

#ifndef AARU_COMPRESSION_NATIVE__LHA_LZSS_H_
#define AARU_COMPRESSION_NATIVE__LHA_LZSS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct lha_lzss
{
    uint8_t *window;
    size_t   mask;
    int64_t  position;
    uint8_t *out_buf;
    size_t   out_pos;
    size_t   out_size;
} lha_lzss;

bool lha_lzss_init(lha_lzss *self, size_t window_size, uint8_t *out_buf, size_t out_size);
void lha_lzss_cleanup(lha_lzss *self);

static inline void lha_lzss_emit_literal(lha_lzss *self, uint8_t literal)
{
    self->window[self->position & self->mask] = literal;
    self->position++;

    if(self->out_pos < self->out_size) self->out_buf[self->out_pos++] = literal;
}

static inline void lha_lzss_emit_match(lha_lzss *self, int offset, int length)
{
    size_t windowoffs = (size_t)(self->position & self->mask);
    int    i;

    for(i = 0; i < length; i++)
    {
        uint8_t byte                                    = self->window[(windowoffs + i - offset) & self->mask];
        self->window[(windowoffs + i) & self->mask] = byte;

        if(self->out_pos < self->out_size) self->out_buf[self->out_pos++] = byte;
    }

    self->position += length;
}

static inline uint8_t lha_lzss_get_byte(lha_lzss *self, int64_t pos)
{
    return self->window[pos & self->mask];
}

#endif /* AARU_COMPRESSION_NATIVE__LHA_LZSS_H_ */
