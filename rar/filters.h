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

#ifndef AARU_COMPRESSION_NATIVE_RAR_FILTERS_H
#define AARU_COMPRESSION_NATIVE_RAR_FILTERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vm.h"

/*
 * Post-decompression filters for RAR 2.0, 3.0, and 5.0.
 *
 * RAR 3.0: fingerprint-based dispatch to native fast paths, VM fallback.
 * RAR 5.0: 4 native filter types (Delta, E8, E8E9, ARM), no VM.
 * RAR 2.0: multichannel audio prediction.
 */

/* RAR 3.0 filter fingerprints for native fast-path dispatch */
#define RAR_FP_DELTA 0x1d0e06077dULL
#define RAR_FP_AUDIO 0xd8bc85e701ULL
#define RAR_FP_E8    0x35ad576887ULL
#define RAR_FP_E8E9  0x393cd7e57eULL

/* RAR 5.0 filter types */
#define RAR5_FILTER_DELTA 0
#define RAR5_FILTER_E8    1
#define RAR5_FILTER_E8E9  2
#define RAR5_FILTER_ARM   3

/* Audio prediction state for RAR 2.0 */
typedef struct
{
    int weight1, weight2, weight3, weight4, weight5;
    int delta1, delta2, delta3, delta4;
    int lastdelta;
    int error[11];
    int count;
    int lastbyte;
} rar_audio20_state_t;

/* Audio prediction state for RAR 3.0 */
typedef struct
{
    int weight1, weight2, weight3;
    int delta1, delta2, delta3;
    int lastdelta;
    int error[7];
    int count;
    int lastbyte;
} rar_audio30_state_t;

/* RAR 3.0 filter instance */
typedef struct rar_filter30_t
{
    rar_vm_invocation_t   *invocation;
    int64_t                block_start;
    int                    block_length;
    uint32_t               filtered_addr;
    uint32_t               filtered_len;
    struct rar_filter30_t *next;
} rar_filter30_t;

/* RAR 5.0 filter instance */
typedef struct rar_filter50_t
{
    int64_t                start;
    uint32_t               length;
    int                    type;
    int                    channels; /* for Delta filter */
    struct rar_filter50_t *next;
} rar_filter50_t;

/* --- Filter effects (pure data transforms) --- */

void rar_filter_delta(const uint8_t *src, uint8_t *dest, size_t length, int channels);
void rar_filter_e8e9(uint8_t *data, size_t length, int64_t file_pos, bool handle_e9, bool wrap_position);
void rar_filter_arm(uint8_t *data, size_t length, int64_t file_pos);

/* --- Audio prediction decoders --- */

int rar_audio20_decode(rar_audio20_state_t *state, int *channel_delta, int delta);
int rar_audio30_decode(rar_audio30_state_t *state, int delta);

/* --- RAR 3.0 filter management --- */

rar_filter30_t *rar_filter30_create(rar_vm_invocation_t *inv, int64_t start, int length);
void            rar_filter30_execute(rar_filter30_t *filter, rar_vm_t *vm, int64_t pos);
void            rar_filter30_free(rar_filter30_t *filter);
void            rar_filter30_free_chain(rar_filter30_t *chain);

/* --- RAR 5.0 filter management --- */

rar_filter50_t *rar_filter50_create(int64_t start, uint32_t length, int type, int channels);
void            rar_filter50_execute(rar_filter50_t *filter, uint8_t *data, size_t data_len, int64_t file_pos);
void            rar_filter50_free(rar_filter50_t *filter);
void            rar_filter50_free_chain(rar_filter50_t *chain);

#endif /* AARU_COMPRESSION_NATIVE_RAR_FILTERS_H */
