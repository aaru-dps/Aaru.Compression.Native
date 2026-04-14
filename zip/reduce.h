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

#ifndef AARU_COMPRESSION_NATIVE_ZIP_REDUCE_H
#define AARU_COMPRESSION_NATIVE_ZIP_REDUCE_H

#include <stddef.h>
#include <stdint.h>

/**
 * Decompress ZIP Reduce (methods 2-5) data.
 *
 * ZIP Reduce uses a two-stage compression: probabilistic follower sets
 * followed by LZ77 with DLE (0x90) escape codes.
 *
 * @param in_buf       Compressed input buffer
 * @param in_len       Length of compressed input
 * @param out_buf      Decompressed output buffer (must be pre-allocated)
 * @param out_len      On entry: size of output buffer. On exit: bytes actually written.
 * @param comp_factor  Compression factor 1-4 (from ZIP method 2-5: factor = method - 1)
 * @return 0 on success, non-zero on error
 */
int zip_reduce_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len, int comp_factor);

#endif /* AARU_COMPRESSION_NATIVE_ZIP_REDUCE_H */
