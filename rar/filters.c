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

#include "filters.h"

#include <stdlib.h>
#include <string.h>

static inline int iabs(int x) { return x < 0 ? -x : x; }

/* ═══════════════════════════════════════════════════════════════════════════
 * Pure data-transform filters
 * ═══════════════════════════════════════════════════════════════════════════ */

void rar_filter_delta(const uint8_t *src, uint8_t *dest, size_t length, int channels)
{
    for(int ch = 0; ch < channels; ch++)
    {
        uint8_t last = 0;
        for(size_t offs = (size_t)ch; offs < length; offs += (size_t)channels)
        {
            uint8_t b  = (uint8_t)(last - *src++);
            dest[offs] = b;
            last       = b;
        }
    }
}

void rar_filter_e8e9(uint8_t *data, size_t length, int64_t file_pos, bool handle_e9, bool wrap_position)
{
    if(length < 5) return;

    int32_t filesize = 0x1000000;

    for(size_t i = 0; i <= length - 5; i++)
    {
        if(data[i] == 0xE8 || (handle_e9 && data[i] == 0xE9))
        {
            int32_t currpos = (int32_t)(file_pos + (int64_t)i + 1);
            if(wrap_position) currpos %= filesize;

            int32_t addr = (int32_t)((uint32_t)data[i + 1] | ((uint32_t)data[i + 2] << 8) |
                                     ((uint32_t)data[i + 3] << 16) | ((uint32_t)data[i + 4] << 24));
            if(addr < 0)
            {
                if(addr + currpos >= 0)
                {
                    uint32_t v  = (uint32_t)(addr + filesize);
                    data[i + 1] = (uint8_t)v;
                    data[i + 2] = (uint8_t)(v >> 8);
                    data[i + 3] = (uint8_t)(v >> 16);
                    data[i + 4] = (uint8_t)(v >> 24);
                }
            }
            else
            {
                if(addr < filesize)
                {
                    uint32_t v  = (uint32_t)(addr - currpos);
                    data[i + 1] = (uint8_t)v;
                    data[i + 2] = (uint8_t)(v >> 8);
                    data[i + 3] = (uint8_t)(v >> 16);
                    data[i + 4] = (uint8_t)(v >> 24);
                }
            }
            i += 4;
        }
    }
}

void rar_filter_arm(uint8_t *data, size_t length, int64_t file_pos)
{
    if(length < 4) return;

    for(size_t i = 0; i <= length - 4; i += 4)
    {
        if(data[i + 3] == 0xEB)
        {
            uint32_t offset = (uint32_t)data[i] | ((uint32_t)data[i + 1] << 8) | ((uint32_t)data[i + 2] << 16);
            offset -= (uint32_t)(file_pos + (int64_t)i) / 4;
            data[i]     = (uint8_t)offset;
            data[i + 1] = (uint8_t)(offset >> 8);
            data[i + 2] = (uint8_t)(offset >> 16);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Audio prediction decoders
 * ═══════════════════════════════════════════════════════════════════════════ */

int rar_audio20_decode(rar_audio20_state_t *state, int *channel_delta, int delta)
{
    state->count++;

    state->delta4 = state->delta3;
    state->delta3 = state->delta2;
    state->delta2 = state->lastdelta - state->delta1;
    state->delta1 = state->lastdelta;

    int predbyte =
        ((8 * state->lastbyte + state->weight1 * state->delta1 + state->weight2 * state->delta2 +
          state->weight3 * state->delta3 + state->weight4 * state->delta4 + state->weight5 * *channel_delta) >>
         3) &
        0xff;

    int byte = (predbyte - delta) & 0xff;

    int prederror = ((int8_t)delta) << 3;

    state->error[0] += iabs(prederror);
    state->error[1] += iabs(prederror - state->delta1);
    state->error[2] += iabs(prederror + state->delta1);
    state->error[3] += iabs(prederror - state->delta2);
    state->error[4] += iabs(prederror + state->delta2);
    state->error[5] += iabs(prederror - state->delta3);
    state->error[6] += iabs(prederror + state->delta3);
    state->error[7] += iabs(prederror - state->delta4);
    state->error[8] += iabs(prederror + state->delta4);
    state->error[9] += iabs(prederror - *channel_delta);
    state->error[10] += iabs(prederror + *channel_delta);

    *channel_delta = state->lastdelta = (int8_t)(byte - state->lastbyte);
    state->lastbyte                   = byte;

    if((state->count & 0x1f) == 0)
    {
        int minerror = state->error[0];
        int minindex = 0;
        for(int i = 1; i < 11; i++)
        {
            if(state->error[i] < minerror)
            {
                minerror = state->error[i];
                minindex = i;
            }
        }
        memset(state->error, 0, sizeof(state->error));

        switch(minindex)
        {
            case 1:
                if(state->weight1 >= -16) state->weight1--;
                break;
            case 2:
                if(state->weight1 < 16) state->weight1++;
                break;
            case 3:
                if(state->weight2 >= -16) state->weight2--;
                break;
            case 4:
                if(state->weight2 < 16) state->weight2++;
                break;
            case 5:
                if(state->weight3 >= -16) state->weight3--;
                break;
            case 6:
                if(state->weight3 < 16) state->weight3++;
                break;
            case 7:
                if(state->weight4 >= -16) state->weight4--;
                break;
            case 8:
                if(state->weight4 < 16) state->weight4++;
                break;
            case 9:
                if(state->weight5 >= -16) state->weight5--;
                break;
            case 10:
                if(state->weight5 < 16) state->weight5++;
                break;
        }
    }

    return byte;
}

int rar_audio30_decode(rar_audio30_state_t *state, int delta)
{
    state->delta3 = state->delta2;
    state->delta2 = state->lastdelta - state->delta1;
    state->delta1 = state->lastdelta;

    int predbyte = ((8 * state->lastbyte + state->weight1 * state->delta1 + state->weight2 * state->delta2 +
                     state->weight3 * state->delta3) >>
                    3) &
                   0xff;

    int byte = (predbyte - delta) & 0xff;

    int prederror = ((int8_t)delta) << 3;

    state->error[0] += iabs(prederror);
    state->error[1] += iabs(prederror - state->delta1);
    state->error[2] += iabs(prederror + state->delta1);
    state->error[3] += iabs(prederror - state->delta2);
    state->error[4] += iabs(prederror + state->delta2);
    state->error[5] += iabs(prederror - state->delta3);
    state->error[6] += iabs(prederror + state->delta3);

    state->lastdelta = (int8_t)(byte - state->lastbyte);
    state->lastbyte  = byte;

    if((state->count & 0x1f) == 0)
    {
        int minerror = state->error[0];
        int minindex = 0;
        for(int i = 1; i < 7; i++)
        {
            if(state->error[i] < minerror)
            {
                minerror = state->error[i];
                minindex = i;
            }
        }
        memset(state->error, 0, sizeof(state->error));

        switch(minindex)
        {
            case 1:
                if(state->weight1 >= -16) state->weight1--;
                break;
            case 2:
                if(state->weight1 < 16) state->weight1++;
                break;
            case 3:
                if(state->weight2 >= -16) state->weight2--;
                break;
            case 4:
                if(state->weight2 < 16) state->weight2++;
                break;
            case 5:
                if(state->weight3 >= -16) state->weight3--;
                break;
            case 6:
                if(state->weight3 < 16) state->weight3++;
                break;
        }
    }

    state->count++;

    return byte;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RAR 3.0 filter management
 * ═══════════════════════════════════════════════════════════════════════════ */

rar_filter30_t *rar_filter30_create(rar_vm_invocation_t *inv, int64_t start, int length)
{
    rar_filter30_t *f = calloc(1, sizeof(rar_filter30_t));
    if(!f) return NULL;
    f->invocation   = inv;
    f->block_start  = start;
    f->block_length = length;
    return f;
}

void rar_filter30_execute(rar_filter30_t *filter, rar_vm_t *vm, int64_t pos)
{
    rar_vm_invocation_t *inv  = filter->invocation;
    rar_vm_program_t    *prog = inv->program;

    rar_vm_invocation_restore_global(inv);

    inv->initial_registers[6] = (uint32_t)pos;
    rar_vm_invocation_set_global32(inv, 0x24, (uint32_t)pos);
    rar_vm_invocation_set_global32(inv, 0x28, (uint32_t)(pos >> 32));

    uint64_t fp = prog->fingerprint;

    if(fp == RAR_FP_DELTA)
    {
        int length      = (int)inv->initial_registers[4];
        int numchannels = (int)inv->initial_registers[0];

        if(length > (int)RAR_VM_WORK_SIZE / 2) return;

        filter->filtered_addr = (uint32_t)length;
        filter->filtered_len  = (uint32_t)length;

        rar_filter_delta(&vm->memory[0], &vm->memory[length], (size_t)length, numchannels);
    }
    else if(fp == RAR_FP_AUDIO)
    {
        int length      = (int)inv->initial_registers[4];
        int numchannels = (int)inv->initial_registers[0];

        if(length > (int)RAR_VM_WORK_SIZE / 2) return;

        filter->filtered_addr = (uint32_t)length;
        filter->filtered_len  = (uint32_t)length;

        uint8_t *src  = &vm->memory[0];
        uint8_t *dest = &vm->memory[length];
        for(int ch = 0; ch < numchannels; ch++)
        {
            rar_audio30_state_t state;
            memset(&state, 0, sizeof(state));
            for(int offs = ch; offs < length; offs += numchannels)
                dest[offs] = (uint8_t)rar_audio30_decode(&state, *src++);
        }
    }
    else if(fp == RAR_FP_E8)
    {
        int length = (int)inv->initial_registers[4];
        if(length > (int)RAR_VM_WORK_SIZE || length < 4) return;

        filter->filtered_addr = 0;
        filter->filtered_len  = (uint32_t)length;

        rar_filter_e8e9(vm->memory, (size_t)length, pos, false, false);
    }
    else if(fp == RAR_FP_E8E9)
    {
        int length = (int)inv->initial_registers[4];
        if(length > (int)RAR_VM_WORK_SIZE || length < 4) return;

        filter->filtered_addr = 0;
        filter->filtered_len  = (uint32_t)length;

        rar_filter_e8e9(vm->memory, (size_t)length, pos, true, false);
    }
    else
    {
        /* Generic VM execution fallback */
        if(!rar_vm_invocation_execute(inv, vm)) return;

        filter->filtered_addr = rar_vm_mem_read32(vm, RAR_VM_SYS_GLOBAL + 0x20) & RAR_VM_MEM_MASK;
        filter->filtered_len  = rar_vm_mem_read32(vm, RAR_VM_SYS_GLOBAL + 0x1c) & RAR_VM_MEM_MASK;

        if(filter->filtered_addr + filter->filtered_len >= RAR_VM_MEM_SIZE)
        {
            filter->filtered_addr = 0;
            filter->filtered_len  = 0;
        }
    }

    rar_vm_invocation_backup_global(inv);
}

void rar_filter30_free(rar_filter30_t *filter)
{
    if(!filter) return;
    rar_vm_invocation_free(filter->invocation);
    free(filter);
}

void rar_filter30_free_chain(rar_filter30_t *chain)
{
    while(chain)
    {
        rar_filter30_t *next = chain->next;
        rar_filter30_free(chain);
        chain = next;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RAR 5.0 filter management
 * ═══════════════════════════════════════════════════════════════════════════ */

rar_filter50_t *rar_filter50_create(int64_t start, uint32_t length, int type, int channels)
{
    rar_filter50_t *f = calloc(1, sizeof(rar_filter50_t));
    if(!f) return NULL;
    f->start    = start;
    f->length   = length;
    f->type     = type;
    f->channels = channels;
    return f;
}

void rar_filter50_execute(rar_filter50_t *filter, uint8_t *data, size_t data_len, int64_t file_pos)
{
    switch(filter->type)
    {
        case RAR5_FILTER_DELTA:
        {
            uint8_t *tmp = malloc(data_len);
            if(!tmp) return;
            rar_filter_delta(data, tmp, data_len, filter->channels);
            memcpy(data, tmp, data_len);
            free(tmp);
            break;
        }
        case RAR5_FILTER_E8:
            rar_filter_e8e9(data, data_len, file_pos, false, true);
            break;
        case RAR5_FILTER_E8E9:
            rar_filter_e8e9(data, data_len, file_pos, true, true);
            break;
        case RAR5_FILTER_ARM:
            rar_filter_arm(data, data_len, file_pos);
            break;
    }
}

void rar_filter50_free(rar_filter50_t *filter) { free(filter); }

void rar_filter50_free_chain(rar_filter50_t *chain)
{
    while(chain)
    {
        rar_filter50_t *next = chain->next;
        free(chain);
        chain = next;
    }
}
