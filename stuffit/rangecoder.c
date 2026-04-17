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

#include "rangecoder.h"

static inline uint8_t rc_read_byte(StuffitRangeCoder *self)
{
    if(self->pos < self->data_len) return self->data[self->pos++];
    return 0;
}

void stuffit_rc_init(StuffitRangeCoder *self, const uint8_t *data, size_t data_len, size_t *pos, bool use_low,
                     uint32_t bottom)
{
    self->data     = data;
    self->data_len = data_len;
    self->pos      = pos ? *pos : 0;
    self->low      = 0;
    self->code     = 0;
    self->range    = 0xffffffff;
    self->use_low  = use_low;
    self->bottom   = bottom;

    for(int i = 0; i < 4; i++) self->code = (self->code << 8) | rc_read_byte(self);
}

uint32_t stuffit_rc_current_count(StuffitRangeCoder *self, uint32_t scale)
{
    self->range /= scale;
    return (self->code - self->low) / self->range;
}

void stuffit_rc_remove_subrange(StuffitRangeCoder *self, uint32_t lowcount, uint32_t highcount)
{
    if(self->use_low)
        self->low += self->range * lowcount;
    else
        self->code -= self->range * lowcount;

    self->range *= highcount - lowcount;

    stuffit_rc_normalize(self);
}

void stuffit_rc_normalize(StuffitRangeCoder *self)
{
    for(;;)
    {
        if((self->low ^ (self->low + self->range)) >= 0x1000000)
        {
            if(self->range >= self->bottom) break;
            self->range = -(int32_t)self->low & (self->bottom - 1);
        }

        self->code = (self->code << 8) | rc_read_byte(self);
        self->range <<= 8;
        self->low <<= 8;
    }
}

int stuffit_rc_next_symbol(StuffitRangeCoder *self, uint32_t *freqtable, int numfreq)
{
    uint32_t total = 0;
    for(int i = 0; i < numfreq; i++) total += freqtable[i];

    uint32_t frequency  = stuffit_rc_current_count(self, total);
    uint32_t cumulative = 0;
    int      n          = 0;

    while(n < numfreq - 1 && cumulative + freqtable[n] <= frequency) cumulative += freqtable[n++];

    stuffit_rc_remove_subrange(self, cumulative, cumulative + freqtable[n]);
    return n;
}

int stuffit_rc_next_bit(StuffitRangeCoder *self)
{
    int bit = stuffit_rc_current_count(self, 2);

    if(bit == 0)
        stuffit_rc_remove_subrange(self, 0, 1);
    else
        stuffit_rc_remove_subrange(self, 1, 2);

    return bit;
}

int stuffit_rc_next_weighted_bit(StuffitRangeCoder *self, int weight, int size)
{
    uint32_t val = stuffit_rc_current_count(self, size);

    if((int)val < weight)
    {
        stuffit_rc_remove_subrange(self, 0, weight);
        return 0;
    }
    else
    {
        stuffit_rc_remove_subrange(self, weight, size);
        return 1;
    }
}

int stuffit_rc_next_weighted_bit2(StuffitRangeCoder *self, int weight, int shift)
{
    uint32_t threshold = (self->range >> shift) * weight;
    int      bit;

    if(self->code < threshold)
    {
        bit         = 0;
        self->range = threshold;
    }
    else
    {
        bit = 1;
        self->range -= threshold;
        self->code -= threshold;
    }

    stuffit_rc_normalize(self);
    return bit;
}
