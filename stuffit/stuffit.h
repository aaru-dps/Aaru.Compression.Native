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

#ifndef AARU_STUFFIT_H
#define AARU_STUFFIT_H

#include <stddef.h>
#include <stdint.h>

/* Classic StuffIt methods */
int stuffit_rle90_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffit_compress_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffit_huffman_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffit_lzah_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffit_mw_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffit_method13_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffit_method14_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffit_arsenic_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);

/* StuffIt X compression methods */
int stuffitx_brimstone_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size, int max_order,
                                     int sub_alloc_size);
int stuffitx_cyanide_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffitx_darkhorse_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size,
                                     int window_bits);
int stuffitx_deflate_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffitx_blend_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffitx_iron_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);

/* StuffIt X preprocessors */
int stuffitx_x86_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);
int stuffitx_english_decode_buffer(uint8_t *dst, size_t *dst_size, const uint8_t *src, size_t src_size);

#endif /* AARU_STUFFIT_H */
