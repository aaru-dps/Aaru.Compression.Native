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

#ifndef AARU_COMPRESSION_NATIVE__ARJ_ARJ_H_
#define AARU_COMPRESSION_NATIVE__ARJ_ARJ_H_

#include <stddef.h>
#include <stdint.h>
#include "../library.h"

AARU_EXPORT int AARU_CALL arj_decompress_method1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

AARU_EXPORT int AARU_CALL arj_decompress_method2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

AARU_EXPORT int AARU_CALL arj_decompress_method3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

AARU_EXPORT int AARU_CALL arj_decompress_fastest(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

AARU_EXPORT int AARU_CALL arjz_decompress_method1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len);

AARU_EXPORT int AARU_CALL arjz_decompress_method2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len);

AARU_EXPORT int AARU_CALL arjz_decompress_method3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len);

#endif /* AARU_COMPRESSION_NATIVE__ARJ_ARJ_H_ */
