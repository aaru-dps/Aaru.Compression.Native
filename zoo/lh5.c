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

/* lh5_mem.c
 *
 * In-memory I/O glue for LH5 decompression.
 * Implements mem_getc(), mem_putc(), and the top-level
 * lh5_decompress() entry point.
 */

#include "lh5.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../library.h"

/* Forward-declaration of the decompression driver in lzh.c */
extern int lzh_decode(void);

/* Buffer I/O state (see lh5_mem.h for externs) */
const uint8_t *in_ptr;
size_t         in_left;
uint8_t       *out_ptr;
size_t         out_left;
int            mem_error;

/*
 * mem_getc(): return next compressed byte, or 0 when in_left==0.
 * Never sets mem_error on input underflow.
 */
int mem_getc(void)
{
    if(in_left == 0) { return 0; /* mimic feof → subbitbuf = 0 */ }
    int c = *in_ptr++;
    in_left--;
    return c;
}

/*
 * mem_putc(): write one output byte, set mem_error on overflow.
 */
int mem_putc(int c)
{
    if(out_left == 0)
    {
        mem_error = 1;
        return EOF;
    }
    *out_ptr++ = (uint8_t)c;
    out_left--;
    return c;
}

/*
 * Top-level in-memory decompressor.
 *
 * in_buf  points to 'in_len' bytes of compressed data.
 * out_buf must have at least *out_len bytes available.
 * On return *out_len is set to the number of bytes written.
 *
 * Returns  0 on success
 *         -1 on error (bad stream or output too small)
 */
AARU_EXPORT int AARU_CALL lh5_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len)
{
    /* Initialize buffer pointers and error flag */
    in_ptr    = in_buf;
    in_left   = in_len;
    out_ptr   = out_buf;
    out_left  = *out_len;
    mem_error = 0;

    /* Invoke the core LH5 decode routine (now buffer-based) */
    if(lzh_decode() != 0 || mem_error) { return -1; }

    /* Compute actual output size */
    *out_len = (size_t)(out_ptr - out_buf);
    return 0;
}
