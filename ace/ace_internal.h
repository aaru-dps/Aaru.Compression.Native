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

#ifndef AARU_ACE_INTERNAL_H
#define AARU_ACE_INTERNAL_H

#include "ace.h"

/* bitstream.c */
void ace_fill_read_buf(ace_decompress_ctx_t *ctx);
void ace_add_bits(ace_decompress_ctx_t *ctx, int bits);
void ace_init_read_buf(ace_decompress_ctx_t *ctx);

/* huffman.c */
int ace_make_codes(ace_decompress_ctx_t *ctx, unsigned max_width, unsigned tab_size, uint16_t *widths, uint16_t *codes);
int ace_read_widths(ace_decompress_ctx_t *ctx, unsigned max_width, uint16_t *codes, uint16_t *widths,
                    unsigned max_size);

/* lz77.c */
void ace_lz77_write_char(ace_decompress_ctx_t *ctx, uint8_t ch);
void ace_lz77_copy_string(ace_decompress_ctx_t *ctx, uint32_t dist, int len);
int  ace_lz77_calc_huff_tabs(ace_decompress_ctx_t *ctx);
int  ace_lz77_read_symbols(ace_decompress_ctx_t *ctx);
void ace_lz77_block_core(ace_decompress_ctx_t *ctx);
int  ace_lz77_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len);
void ace_lz77_copy_to_dict(ace_decompress_ctx_t *ctx, const uint8_t *buf, int len);

/* sound.c */
int  ace_sound_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len);
void ace_sound_init(ace_decompress_ctx_t *ctx, int type);

/* pic.c */
int  ace_pic_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len);
void ace_pic_init(ace_decompress_ctx_t *ctx);
void ace_pic_done(ace_decompress_ctx_t *ctx);

/* v2.c */
int ace_decompress_v20_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len);
int ace_lz77_preprocess_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len);

/* ace.c (init/bitwidth) */
void ace_init_bitwidth(ace_decompress_ctx_t *ctx);
int  ace_get_bit_width(ace_decompress_ctx_t *ctx, int value);

#endif /* AARU_ACE_INTERNAL_H */
