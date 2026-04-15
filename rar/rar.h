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

#ifndef AARU_COMPRESSION_NATIVE_RAR_RAR_H
#define AARU_COMPRESSION_NATIVE_RAR_RAR_H

#include <stddef.h>
#include <stdint.h>

#include "../library.h"

/*
 * RAR decompression public API.
 *
 * All functions decompress a single compressed data stream (without archive
 * headers) into an output buffer. The caller provides the expected decompressed
 * size in *out_len and receives the actual size on return.
 *
 * Returns 0 on success, non-zero on error.
 */

/* RAR 1.5 (UNP_VER=15): Custom LZ77 with fixed Huffman tables, 64KB window */
AARU_EXPORT int AARU_CALL rar15_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

/* RAR 2.0 (UNP_VER=20): Block Huffman LZ77 with optional audio, 1MB window */
AARU_EXPORT int AARU_CALL rar20_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

/* RAR 3.0 (UNP_VER=29): Huffman LZ77 / PPMd Variant H + VM filters, 4MB window */
AARU_EXPORT int AARU_CALL rar30_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

/* RAR 5.0: Huffman LZ77 + native filters, variable window */
AARU_EXPORT int AARU_CALL rar50_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len,
                                           size_t window_size);

#endif /* AARU_COMPRESSION_NATIVE_RAR_RAR_H */
