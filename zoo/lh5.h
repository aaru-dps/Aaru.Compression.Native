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

/* lh5_mem.h
 *
 * In-memory I/O glue for LH5 decompression.
 * Defines the mem_getc()/mem_putc() buffer readers/writers
 * and declares the lh5_decompress() entry point.
 */
#ifndef AARU_COMPRESSION_NATIVE_LH5_H
#define AARU_COMPRESSION_NATIVE_LH5_H

#include <stddef.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * State for in-memory I/O
 * --------------------------------------------------------------------------
 * in_ptr/in_left:   where to read next compressed byte
 * out_ptr/out_left: where to write next decompressed byte
 * mem_error:        set to 1 on underflow/overflow
 */
extern const uint8_t *in_ptr;
extern size_t         in_left;
extern uint8_t       *out_ptr;
extern size_t         out_left;
extern int            mem_error;

/* --------------------------------------------------------------------------
 * Fetch one byte from in_buf; returns EOF on underflow
 * -------------------------------------------------------------------------- */
int mem_getc(void);

/* --------------------------------------------------------------------------
 * Write one byte into out_buf; returns c or EOF on overflow
 * -------------------------------------------------------------------------- */
int mem_putc(int c);

#endif  // AARU_COMPRESSION_NATIVE_LH5_H
