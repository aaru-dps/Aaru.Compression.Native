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

#ifndef AARU_STUFFIT_BWT_H
#define AARU_STUFFIT_BWT_H

#include <stdbool.h>
#include <stdint.h>

/* Inverse BWT transform */
void stuffit_calculate_inverse_bwt(uint32_t *transform, uint8_t *block, int blocklen);
void stuffit_unsort_bwt(uint8_t *dest, uint8_t *src, int blocklen, int firstindex, uint32_t *transform);

/* ST4 (Scrambled Transformation v4) inverse */
bool stuffit_unsort_st4(uint8_t *dest, uint8_t *src, int blocklen, int firstindex, uint32_t *transform);

/* Move-To-Front decoder */
typedef struct StuffitMTFState
{
    int table[256];
} StuffitMTFState;

void stuffit_reset_mtf(StuffitMTFState *self);
int  stuffit_decode_mtf(StuffitMTFState *self, int symbol);
void stuffit_decode_mtf_block(uint8_t *block, int blocklen);

/* M1FF-N block decoder (used by Cyanide) */
void stuffit_decode_m1ff_block(uint8_t *block, int blocklen, int order);

#endif /* AARU_STUFFIT_BWT_H */
