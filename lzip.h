/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2021 Natalia Portillo.
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

#ifndef AARU_CHECKSUMS_NATIVE__LZIP_H_
#define AARU_CHECKSUMS_NATIVE__LZIP_H_

AARU_EXPORT int32_t AARU_CALL lzip_decode_buffer(uint8_t*       dst_buffer,
                                                 int32_t        dst_size,
                                                 const uint8_t* src_buffer,
                                                 int32_t        src_size);

AARU_EXPORT int32_t AARU_CALL lzip_encode_buffer(uint8_t*       dst_buffer,
                                                 int32_t        dst_size,
                                                 const uint8_t* src_buffer,
                                                 int32_t        src_size,
                                                 int32_t dictionary_size,
                                                 int32_t match_len_limit);
#endif // AARU_CHECKSUMS_NATIVE__LZIP_H_
