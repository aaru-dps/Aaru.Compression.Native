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

#ifndef AARU_COMPRESSION_NATIVE_ZIP_IMPLODE_H
#define AARU_COMPRESSION_NATIVE_ZIP_IMPLODE_H

#include <stddef.h>
#include <stdint.h>

/**
 * Decompress ZIP Implode (method 6) data.
 *
 * ZIP Implode uses Shannon-Fano coded LZSS with LSB-first bitstream.
 *
 * @param in_buf           Compressed input buffer
 * @param in_len           Length of compressed input
 * @param out_buf          Decompressed output buffer (must be pre-allocated)
 * @param out_len          On entry: size of output buffer. On exit: bytes actually written.
 * @param large_dictionary Non-zero for 8K sliding dictionary (flag bit 1), zero for 4K
 * @param has_literals     Non-zero if literal tree is present (flag bit 2)
 * @return 0 on success, non-zero on error
 */
int zip_implode_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len,
                           int large_dictionary, int has_literals);

#endif /* AARU_COMPRESSION_NATIVE_ZIP_IMPLODE_H */
