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

#include "blast_buf.h"
#include "blast.h"

#include <string.h>

/* Context for buffer-based input callback */
struct blast_in_ctx
{
    const uint8_t *buf;
    size_t         len;
    int            done;
};

/* Context for buffer-based output callback */
struct blast_out_ctx
{
    uint8_t *buf;
    size_t   len;
    size_t   pos;
};

/* Input callback: provide all data in one shot */
static unsigned blast_in_buf(void *how, unsigned char **buf)
{
    struct blast_in_ctx *ctx = (struct blast_in_ctx *)how;

    if(ctx->done) return 0;

    *buf      = (unsigned char *)ctx->buf;
    ctx->done = 1;
    return (unsigned)ctx->len;
}

/* Output callback: copy to output buffer */
static int blast_out_buf(void *how, unsigned char *buf, unsigned len)
{
    struct blast_out_ctx *ctx = (struct blast_out_ctx *)how;

    if(ctx->pos + len > ctx->len) return 1; /* output buffer overflow */

    memcpy(ctx->buf + ctx->pos, buf, len);
    ctx->pos += len;
    return 0;
}

int zip_blast_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    struct blast_in_ctx  in_ctx  = {in_buf, in_len, 0};
    struct blast_out_ctx out_ctx = {out_buf, *out_len, 0};

    int err = blast(blast_in_buf, &in_ctx, blast_out_buf, &out_ctx, NULL, NULL);

    *out_len = out_ctx.pos;
    return err;
}
