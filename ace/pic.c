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

#include <stdlib.h>
#include <string.h>

#include "ace.h"
#include "ace_internal.h"

static int ace_pic_golomb_rice(ace_decompress_ctx_t *ctx, int k)
{
    int i, err;

    err = k ? (int)(ctx->read_code >> (32 - k)) : 0;
    ace_add_bits(ctx, k);

    do
    {
        i = ctx->read_code >> 31;
        if(i) err += 1 << k;
        ace_add_bits(ctx, 1);
    } while(i);

    return err;
}

static int ace_pic_get_context(ace_decompress_ctx_t *ctx)
{
    return ACE_PIC_QUANT_X9X9(ctx, ctx->pic_pixel_d - ctx->pic_pixel_a) +
           ACE_PIC_QUANT_X9(ctx, ctx->pic_pixel_a - ctx->pic_pixel_c) +
           ACE_PIC_QUANT(ctx, ctx->pic_pixel_c - ctx->pic_pixel_b);
}

static void ace_pic_init_model(ace_decompress_ctx_t *ctx)
{
    int pred, i;
    for(pred = 0; pred <= 1; pred++)
    {
        memset(&ctx->pic_context[pred], 0, sizeof(ctx->pic_context[0]));
        for(i = 0; i < ACE_PIC_CONTEXT_NUMBER; i++) ctx->pic_context[pred][i].average_counter = 4;
    }
}

static void ace_pic_init_quantizers(ace_decompress_ctx_t *ctx)
{
    int i;
    for(i = -255; i <= 255; i++)
    {
        if(i <= -ACE_PIC_S3)
            ACE_PIC_QUANT(ctx, i) = -4;
        else if(i <= -ACE_PIC_S2)
            ACE_PIC_QUANT(ctx, i) = -3;
        else if(i <= -ACE_PIC_S1)
            ACE_PIC_QUANT(ctx, i) = -2;
        else if(i <= -1)
            ACE_PIC_QUANT(ctx, i) = -1;
        else if(!i)
            ACE_PIC_QUANT(ctx, i) = 0;
        else if(i < ACE_PIC_S1)
            ACE_PIC_QUANT(ctx, i) = 1;
        else if(i < ACE_PIC_S2)
            ACE_PIC_QUANT(ctx, i) = 2;
        else if(i < ACE_PIC_S3)
            ACE_PIC_QUANT(ctx, i) = 3;
        else
            ACE_PIC_QUANT(ctx, i) = 4;
    }

    for(i = -255; i <= 255; i++) ACE_PIC_QUANT_X9(ctx, i) = 9 * ACE_PIC_QUANT(ctx, i);
    for(i = -255; i <= 255; i++) ACE_PIC_QUANT_X9X9(ctx, i) = 9 * ACE_PIC_QUANT_X9(ctx, i);
}

static void ace_pic_set_pixels1(ace_decompress_ctx_t *ctx)
{
    ctx->pic_pixel_d = ctx->pic_data[1][ctx->pic_cur_plane];
    ctx->pic_pixel_a = ctx->pic_pixel_b = ctx->pic_pixel_c = ctx->pic_pixel_x = 0;

    if(ctx->pic_cur_pred == 1)
    {
        ctx->pic_pixel_a = ctx->pic_pixel_b = ctx->pic_pixel_c = ctx->pic_pixel_x = 128;
        if(ctx->pic_cur_plane > 0) ctx->pic_pixel_d -= ctx->pic_data[1][ctx->pic_cur_plane - 1] - 128;
    }
    else if(ctx->pic_cur_pred == 2)
    {
        ctx->pic_pixel_a = ctx->pic_pixel_b = ctx->pic_pixel_c = ctx->pic_pixel_x = 128;
        if(ctx->pic_cur_plane > 0) ctx->pic_pixel_d -= (ctx->pic_data[1][ctx->pic_cur_plane - 1] * 11 >> 4) - 128;
    }
}

static void ace_pic_set_pixels2(ace_decompress_ctx_t *ctx)
{
    ctx->pic_pixel_c = ctx->pic_pixel_a;
    ctx->pic_pixel_a = ctx->pic_pixel_d;
    ctx->pic_pixel_b = ctx->pic_pixel_x;
}

static void ace_pic_set_pixels3(ace_decompress_ctx_t *ctx)
{
    ctx->pic_pixel_a = ctx->pic_data[1][ctx->pic_cur_col];
    ctx->pic_pixel_b = ctx->pic_data[0][ctx->pic_cur_col - ctx->pic_planes];
    ctx->pic_pixel_c = ctx->pic_data[1][ctx->pic_cur_col - ctx->pic_planes];

    if(ctx->pic_cur_pred == 1)
    {
        ctx->pic_pixel_a -= ctx->pic_data[1][ctx->pic_cur_col - 1] - 128;
        ctx->pic_pixel_b -= ctx->pic_data[0][ctx->pic_cur_col - ctx->pic_planes - 1] - 128;
        ctx->pic_pixel_c -= ctx->pic_data[1][ctx->pic_cur_col - ctx->pic_planes - 1] - 128;
    }
    else if(ctx->pic_cur_pred == 2)
    {
        ctx->pic_pixel_a -= (ctx->pic_data[1][ctx->pic_cur_col - 1] * 11 >> 4) - 128;
        ctx->pic_pixel_b -= (ctx->pic_data[0][ctx->pic_cur_col - ctx->pic_planes - 1] * 11 >> 4) - 128;
        ctx->pic_pixel_c -= (ctx->pic_data[1][ctx->pic_cur_col - ctx->pic_planes - 1] * 11 >> 4) - 128;
    }
}

static void ace_pic_set_pixel_state(ace_decompress_ctx_t *ctx)
{
    ctx->pic_pixel_d = ctx->pic_data[1][ctx->pic_cur_col + ctx->pic_planes];

    if(ctx->pic_cur_pred == 1)
        ctx->pic_pixel_d -= ctx->pic_data[1][ctx->pic_cur_col + ctx->pic_planes - 1] - 128;
    else if(ctx->pic_cur_pred == 2)
        ctx->pic_pixel_d -= (ctx->pic_data[1][ctx->pic_cur_col + ctx->pic_planes - 1] * 11 >> 4) - 128;

    ctx->pic_cur_state = abs(ace_pic_get_context(ctx));
}

static void ace_pic_pixel(ace_decompress_ctx_t *ctx)
{
    int     k, best_predictor, best_error_count;
    uint8_t m_epsilon, predicted;
    int8_t  epsilon;

    ctx->cur_context[ctx->pic_cur_state].used_counter++;

    k = ctx->bit_width_array[ctx->cur_context[ctx->pic_cur_state].average_counter /
                             ctx->cur_context[ctx->pic_cur_state].used_counter];

    m_epsilon = (uint8_t)ace_pic_golomb_rice(ctx, k);

    if(m_epsilon & 1)
        epsilon = -(int8_t)(m_epsilon / 2) - 1;
    else
        epsilon = (int8_t)(m_epsilon / 2);

    switch(ctx->cur_context[ctx->pic_cur_state].predictor_number)
    {
        case 0:
            predicted = (uint8_t)ctx->pic_pixel_a;
            break;
        case 1:
            predicted = (uint8_t)ctx->pic_pixel_b;
            break;
        case 2:
            predicted = (uint8_t)((ctx->pic_pixel_a + ctx->pic_pixel_b) >> 1);
            break;
        case 3:
            predicted = (uint8_t)(ctx->pic_pixel_a + ctx->pic_pixel_b - ctx->pic_pixel_c);
            break;
        default:
            predicted = 0;
    }

    ctx->pic_pixel_x = (uint8_t)(epsilon + predicted);

    ctx->cur_context[ctx->pic_cur_state].error_counters[0] +=
        ctx->dif_bit_width_array[(uint8_t)(ctx->pic_pixel_x - ctx->pic_pixel_a)];

    best_predictor   = 0;
    best_error_count = ctx->cur_context[ctx->pic_cur_state].error_counters[0];

    ctx->cur_context[ctx->pic_cur_state].error_counters[1] +=
        ctx->dif_bit_width_array[(uint8_t)(ctx->pic_pixel_x - ctx->pic_pixel_b)];

    if(ctx->cur_context[ctx->pic_cur_state].error_counters[1] < best_error_count)
    {
        best_predictor   = 1;
        best_error_count = ctx->cur_context[ctx->pic_cur_state].error_counters[1];
    }

    ctx->cur_context[ctx->pic_cur_state].error_counters[2] +=
        ctx->dif_bit_width_array[(uint8_t)(ctx->pic_pixel_x - ((ctx->pic_pixel_a + ctx->pic_pixel_b) >> 1))];

    if(ctx->cur_context[ctx->pic_cur_state].error_counters[2] < best_error_count)
    {
        best_predictor   = 2;
        best_error_count = ctx->cur_context[ctx->pic_cur_state].error_counters[2];
    }

    ctx->cur_context[ctx->pic_cur_state].error_counters[3] += ctx->dif_bit_width_array[(
        uint8_t)(ctx->pic_pixel_x - (ctx->pic_pixel_a + ctx->pic_pixel_b - ctx->pic_pixel_c))];

    if(ctx->cur_context[ctx->pic_cur_state].error_counters[3] < best_error_count) best_predictor = 3;

    {
        uint32_t *ec = (uint32_t *)&ctx->cur_context[ctx->pic_cur_state].error_counters;
        if(*ec & 0x80808080u) *ec = (*ec >> 1) & 0x7f7f7f7fu;
    }

    ctx->cur_context[ctx->pic_cur_state].predictor_number = (uint8_t)best_predictor;
    ctx->cur_context[ctx->pic_cur_state].average_counter += (uint16_t)abs(epsilon);

    if(ctx->cur_context[ctx->pic_cur_state].used_counter == ACE_PIC_N0)
    {
        ctx->cur_context[ctx->pic_cur_state].used_counter >>= 1;
        ctx->cur_context[ctx->pic_cur_state].average_counter >>= 1;
    }

    switch(ctx->pic_cur_pred)
    {
        case 0:
            ctx->pic_data[0][ctx->pic_cur_col] = (int8_t)ctx->pic_pixel_x;
            break;
        case 1:
            ctx->pic_data[0][ctx->pic_cur_col] =
                (int8_t)(ctx->pic_pixel_x + ctx->pic_data[0][ctx->pic_cur_col - 1] - 128);
            break;
        case 2:
            ctx->pic_data[0][ctx->pic_cur_col] =
                (int8_t)(ctx->pic_pixel_x + (ctx->pic_data[0][ctx->pic_cur_col - 1] * 11 >> 4) - 128);
            break;
    }
}

static void ace_pic_symbol(ace_decompress_ctx_t *ctx)
{
    ace_pic_set_pixels2(ctx);
    ace_pic_set_pixels3(ctx);
    ace_pic_set_pixel_state(ctx);
    ace_pic_pixel(ctx);
}

static void ace_pic_line(ace_decompress_ctx_t *ctx)
{
    int8_t *temp;

    for(ctx->pic_cur_plane = 0; ctx->pic_cur_plane < ctx->pic_planes; ctx->pic_cur_plane++)
    {
        if(ctx->pic_cur_plane)
        {
            ctx->cur_context  = ctx->pic_context[1];
            ctx->pic_cur_pred = ctx->read_code >> (32 - 2);
            ace_add_bits(ctx, 2);
        }
        else
        {
            ctx->cur_context  = ctx->pic_context[0];
            ctx->pic_cur_pred = 0;
        }

        ace_pic_set_pixels1(ctx);

        for(ctx->pic_cur_col = ctx->pic_cur_plane; ctx->pic_cur_col < ctx->pic_width;
            ctx->pic_cur_col += ctx->pic_planes)
        {
            ace_pic_symbol(ctx);
        }
    }

    ctx->pic_data_pos = ctx->pic_width;
    temp              = ctx->pic_data[0];
    ctx->pic_data[0]  = ctx->pic_data[1];
    ctx->pic_data[1]  = temp;
}

void ace_pic_done(ace_decompress_ctx_t *ctx)
{
    if(ctx->pic_data[0])
    {
        free(ctx->pic_data[0] - ctx->pic_planes);
        free(ctx->pic_data[1] - ctx->pic_planes);
        ctx->pic_data[0] = NULL;
        ctx->pic_data[1] = NULL;
    }
}

void ace_pic_init(ace_decompress_ctx_t *ctx)
{
    int     i, j;
    int8_t *buf;

    ace_pic_done(ctx);

    ctx->pic_width  = ace_pic_golomb_rice(ctx, 12);
    ctx->pic_planes = ace_pic_golomb_rice(ctx, 2);

    i = ctx->pic_width + 2 * ctx->pic_planes;

    for(j = 0; j <= 1; j++)
    {
        buf = (int8_t *)malloc(i);
        if(!buf)
        {
            ctx->error = 1;
            return;
        }
        memset(buf, 0, i);
        ctx->pic_data[j] = buf + ctx->pic_planes;
    }

    ace_pic_init_model(ctx);
    ace_pic_init_quantizers(ctx);
    ctx->pic_data_pos = 0;
}

int ace_pic_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len)
{
    int i, rest_len;

    rest_len = len;

    while(rest_len)
    {
        if(!ctx->pic_data_pos)
        {
            if(ctx->file_size)
            {
                i = ctx->read_code >> (32 - 1);
                ace_add_bits(ctx, 1);

                if(!i)
                {
                    ctx->next_type = ctx->read_code >> (32 - 8);
                    ace_add_bits(ctx, 8);

                    switch(ctx->next_type)
                    {
                        case ACE_BLOCK_LZ77_DELTA:
                            ctx->next_delta_dist = ctx->read_code >> (32 - 8);
                            ace_add_bits(ctx, 8);
                            ctx->next_delta_len = ctx->read_code >> (32 - 17);
                            ace_add_bits(ctx, 17);
                            break;
                        case ACE_BLOCK_LZ77_EXE:
                            ctx->next_exe_mode = ctx->read_code >> (32 - 8);
                            ace_add_bits(ctx, 8);
                            break;
                        default:
                            break;
                    }
                    break;
                }
            }
            else
            {
                break;
            }

            ace_pic_line(ctx);
        }

        i = rest_len > ctx->pic_data_pos ? ctx->pic_data_pos : rest_len;
        memcpy(buf, &ctx->pic_data[1][ctx->pic_width - ctx->pic_data_pos], i);

        buf += i;
        rest_len -= i;
        ctx->pic_data_pos -= i;
        ctx->file_size -= i;
    }

    return len - rest_len;
}
