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

#ifndef AARU_COMPRESSION_NATIVE__LHA_LH1_H_
#define AARU_COMPRESSION_NATIVE__LHA_LH1_H_

#include <stdint.h>
#include <stddef.h>
#include "../library.h"

AARU_EXPORT int AARU_CALL lha_decompress_lh1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

#endif /* AARU_COMPRESSION_NATIVE__LHA_LH1_H_ */
