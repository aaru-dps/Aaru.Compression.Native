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

#ifndef AARU_ACE_H
#define AARU_ACE_H

#include <stddef.h>
#include <stdint.h>

/* ACE compression technique types */
#define ACE_TECHNIQUE_STORE   0
#define ACE_TECHNIQUE_LZ77    1 /* ACE v1 LZ77 */
#define ACE_TECHNIQUE_BLOCKED 2 /* ACE v2 blocked */

/* ACE v2 block subtypes */
#define ACE_BLOCK_LZ77_NORM  0
#define ACE_BLOCK_LZ77_DELTA 1
#define ACE_BLOCK_LZ77_EXE   2
#define ACE_BLOCK_SOUND_8    3
#define ACE_BLOCK_SOUND_16   4
#define ACE_BLOCK_SOUND_32_1 5
#define ACE_BLOCK_SOUND_32_2 6
#define ACE_BLOCK_PIC        7

/* Constants */
#define ACE_MAX_DIC_BITS     22
#define ACE_MAX_CODE_WIDTH   11
#define ACE_MAX_LEN          259
#define ACE_MAX_DIST_AT_LEN2 255
#define ACE_MAX_DIST_AT_LEN3 8191
#define ACE_MAX_DIC_BITS2    (ACE_MAX_DIC_BITS / 2)
#define ACE_MAX_DIC_SIZE     (1 << ACE_MAX_DIC_BITS)
#define ACE_MAX_DIST2        (1 << ACE_MAX_DIC_BITS2)

#define ACE_MAX_MAIN_CODE (260 + ACE_MAX_DIC_BITS + 2)
#define ACE_MAX_LEN_CODE  (256 - 1)
#define ACE_TYPE_CODE     (260 + ACE_MAX_DIC_BITS + 1)

#define ACE_MAX_SAVE_WIDTH    7
#define ACE_MAX_WIDTH_TO_SAVE 15

/* Sound constants */
#define ACE_SOUND_RUNLEN_CODES   32
#define ACE_SOUND_MAX_CODE       (256 + ACE_SOUND_RUNLEN_CODES + 1)
#define ACE_SOUND_MAX_CODE_WIDTH 10
#define ACE_SOUND_TYPE_CODE      (256 + ACE_SOUND_RUNLEN_CODES)
#define ACE_SOUND_MAX_CHANNELS   3
#define ACE_SOUND_MAX_MODELS     (ACE_SOUND_MAX_CHANNELS * 3)
#define ACE_SOUND_CHANNEL_BLOCK  2000
#define ACE_SOUND_HISTORY_SIZE   256
#define ACE_SOUND_MAX_BLOCK      (ACE_SOUND_CHANNEL_BLOCK * ACE_SOUND_MAX_MODELS + 8)

/* PIC constants */
#define ACE_PIC_N0             32
#define ACE_PIC_S1             3
#define ACE_PIC_S2             12
#define ACE_PIC_S3             40
#define ACE_PIC_CONTEXT_NUMBER (9 * 9 * 9)

/* Read buffer size in 32-bit words */
#define ACE_READ_BUF_SIZE 8192

/* Max partition size for symbol batching */
#define ACE_MAX_PART_SIZE 1024

/* Max delta block */
#define ACE_MAX_DELTA_BLOCK 65536

/* Max HUFF code (for sort arrays) */
#define ACE_MAX_HUFF_CODE ACE_SOUND_MAX_CODE

/* PIC quantizer array access macros */
#define ACE_PIC_QUANT(ctx, v)      ((ctx)->pic_quantizer[(v) + 255])
#define ACE_PIC_QUANT_X9(ctx, v)   ((ctx)->pic_quantizer_x9[(v) + 255])
#define ACE_PIC_QUANT_X9X9(ctx, v) ((ctx)->pic_quantizer_x9x9[(v) + 255])

/* Sound channel number tables */
extern const int ace_sound_channel_num[4][4];
extern const int ace_sound_models[4];

/* PIC context entry */
typedef struct
{
    uint8_t  error_counters[4];
    uint8_t  predictor_number;
    uint16_t used_counter;
    uint16_t average_counter;
} ace_pic_context_t;

/* Main decompressor context */
typedef struct
{
    /* Input buffer and position */
    const uint8_t *in_buf;
    size_t         in_len;
    size_t         in_pos;

    /* Bit reader state */
    uint32_t read_buf[ACE_READ_BUF_SIZE];
    int      read_buf_pos;
    int      read_code_bit_pos;
    uint32_t read_code;

    /* Dictionary */
    uint8_t *dictionary;
    uint32_t dic_pos;
    uint32_t dic_size;
    uint32_t dic_and;
    int      dic_bits;

    /* File decompression state */
    int64_t file_size;
    int64_t file_pos;

    /* Huffman tables for LZ77 */
    uint16_t main_huff_symbols[(1 << ACE_MAX_CODE_WIDTH) + 1];
    uint16_t main_huff_widths[ACE_MAX_MAIN_CODE + 2];
    uint16_t len_huff_symbols[(1 << ACE_MAX_CODE_WIDTH) + 1];
    uint16_t len_huff_widths[ACE_MAX_LEN_CODE + 3];

    /* Symbol batching buffers */
    uint16_t main_buf[ACE_MAX_PART_SIZE];
    uint16_t len_buf[ACE_MAX_PART_SIZE];
    uint32_t dist_buf[ACE_MAX_PART_SIZE];
    uint32_t main_buf_pos;
    uint32_t len_dist_buf_pos;

    /* LZ77 state */
    int      old_dists_pos;
    uint32_t old_dists[4];
    uint32_t block_size;
    uint32_t part_size;
    uint32_t block_byte_count;
    uint32_t block_buf_size;

    /* V2 blocked state */
    int type;
    int next_type;
    int over_next_type;

    /* Delta preprocessing */
    int      prep_num_kept_bytes;
    int      prep_kept_bytes_pos;
    uint32_t prep_last_delta;
    int      delta_dist;
    int      next_delta_dist;
    int      delta_len;
    int      next_delta_len;
    int      delta_block_size;
    int      delta_plane_size;
    int      delta_plane;
    int      delta_plane_pos;
    uint8_t  prep_kept_bytes_buf[ACE_MAX_DELTA_BLOCK + ACE_MAX_LEN];

    /* EXE preprocessing */
    int exe_mode;
    int next_exe_mode;

    /* Quicksort working arrays */
    uint16_t sort_elements[ACE_MAX_HUFF_CODE + 2];
    uint16_t sort_frequencies[(ACE_MAX_HUFF_CODE + 2) * 2];
    uint16_t save_widths[ACE_MAX_WIDTH_TO_SAVE];

    /* Bitwidth tables */
    uint8_t bit_width_array[ACE_MAX_DIST2];
    uint8_t dif_bit_width_array[256];

    /* Sound decompression */
    uint16_t sound_huff_symbols[ACE_SOUND_MAX_MODELS][(1 << ACE_SOUND_MAX_CODE_WIDTH) + 1];
    uint16_t sound_huff_widths[ACE_SOUND_MAX_MODELS][ACE_SOUND_MAX_CODE + 2];
    int      sound_quantizer[256];

    struct
    {
        int predictor_dif_cnt[ACE_SOUND_MAX_CHANNELS][2];
        int last_predictor_dif_cnt[ACE_SOUND_MAX_CHANNELS][2];
        int rar_dif_cnt[ACE_SOUND_MAX_CHANNELS][4];
        int rar_coefficient[ACE_SOUND_MAX_CHANNELS][4];
        int rar_dif[ACE_SOUND_MAX_CHANNELS][9];
        int byte_count[ACE_SOUND_MAX_CHANNELS];
        int last_byte[ACE_SOUND_MAX_CHANNELS];
        int last_delta[ACE_SOUND_MAX_CHANNELS];
        int state[ACE_SOUND_MAX_CHANNELS];
        int code[ACE_SOUND_MAX_CHANNELS];
        int adaptive_model_cnt[ACE_SOUND_MAX_CHANNELS];
        int adaptive_model_use[ACE_SOUND_MAX_CHANNELS];
        int models;
        int mode;
        int sound_block_size;
    } sound_var;

    /* PIC decompression */
    int                pic_quantizer[511]; /* indexed as [v+255] for v in -255..255 */
    int                pic_quantizer_x9[511];
    int                pic_quantizer_x9x9[511];
    ace_pic_context_t  pic_context[2][ACE_PIC_CONTEXT_NUMBER];
    ace_pic_context_t *cur_context;
    int8_t            *pic_data[2];
    int                pic_width;
    int                pic_planes;
    int                pic_data_pos;
    int                pic_cur_col;
    int                pic_cur_plane;
    int                pic_cur_pred;
    int                pic_cur_state;
    int                pic_pixel_a;
    int                pic_pixel_b;
    int                pic_pixel_c;
    int                pic_pixel_d;
    int                pic_pixel_x;

    /* Error flag */
    int error;
} ace_decompress_ctx_t;

/* Initialize context for decompression */
int ace_decompress_init(ace_decompress_ctx_t *ctx, int dic_bits);

/* Decompress an ACE v1 (LZ77) compressed block */
int ace_decompress_v1(ace_decompress_ctx_t *ctx, const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                      size_t *out_len);

/* Decompress an ACE v2 (blocked) compressed block */
int ace_decompress_v2(ace_decompress_ctx_t *ctx, const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                      size_t *out_len);

/* Free context resources */
void ace_decompress_free(ace_decompress_ctx_t *ctx);

#endif /* AARU_ACE_H */
