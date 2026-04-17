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

#ifndef AARU_STUFFIT_RANGECODER_H
#define AARU_STUFFIT_RANGECODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct StuffitRangeCoder
{
    const uint8_t *data;
    size_t         data_len;
    size_t         pos;
    uint32_t       low, code, range, bottom;
    bool           use_low;
} StuffitRangeCoder;

void stuffit_rc_init(StuffitRangeCoder *self, const uint8_t *data, size_t data_len, size_t *pos, bool use_low,
                     uint32_t bottom);

uint32_t stuffit_rc_current_count(StuffitRangeCoder *self, uint32_t scale);
void     stuffit_rc_remove_subrange(StuffitRangeCoder *self, uint32_t lowcount, uint32_t highcount);
void     stuffit_rc_normalize(StuffitRangeCoder *self);

int stuffit_rc_next_symbol(StuffitRangeCoder *self, uint32_t *freqtable, int numfreq);
int stuffit_rc_next_bit(StuffitRangeCoder *self);
int stuffit_rc_next_weighted_bit(StuffitRangeCoder *self, int weight, int size);
int stuffit_rc_next_weighted_bit2(StuffitRangeCoder *self, int weight, int shift);

#endif /* AARU_STUFFIT_RANGECODER_H */
