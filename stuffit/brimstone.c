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

/* StuffIt X method 0: Brimstone (PPMd Variant G with Brimstone suballocator) */

#include <stdlib.h>
#include "../ppmd/SubAllocatorBrimstone.h"
#include "../ppmd/VariantG.h"
#include "stuffit.h"

typedef struct BrimstoneCtx
{
    const uint8_t *data;
    size_t         len;
    size_t         pos;
} BrimstoneCtx;

static int brimstone_read(void *context)
{
    BrimstoneCtx *ctx = context;
    if(ctx->pos >= ctx->len) return -1;
    return ctx->data[ctx->pos++];
}

int stuffitx_brimstone_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size, int max_order,
                                     int sub_alloc_size)
{
    size_t limit = *dst_size;
    size_t di    = 0;

    BrimstoneCtx ctx = {src, src_size, 0};

    PPMdSubAllocatorBrimstone *alloc = CreateSubAllocatorBrimstone(sub_alloc_size);
    if(!alloc) return -1;

    PPMdModelVariantG model;
    if(!StartPPMdModelVariantG(&model, brimstone_read, &ctx, (PPMdSubAllocator *)alloc, max_order, true))
    {
        FreeSubAllocatorBrimstone(alloc);
        return -1;
    }

    while(di < limit)
    {
        int byte = NextPPMdVariantGByte(&model);
        if(byte < 0) break;
        dst[di++] = (uint8_t)byte;
    }

    FreeSubAllocatorBrimstone(alloc);
    *dst_size = di;
    return 0;
}
