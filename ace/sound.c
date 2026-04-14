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

const int ace_sound_channel_num[4][4] = {
    {0, 0, 0, 0},
    {0, 1, 0, 1},
    {0, 1, 2, 0},
    {0, 1, 2, 0}
};

const int ace_sound_models[4] = {3, 6, 9, 6};

static uint8_t ace_sound_get_predicted(ace_decompress_ctx_t *ctx, int ch)
{
    return (uint8_t)((8 * ctx->sound_var.last_byte[ch] +
                      ctx->sound_var.rar_coefficient[ch][0] * ctx->sound_var.rar_dif_cnt[ch][0] +
                      ctx->sound_var.rar_coefficient[ch][1] * ctx->sound_var.rar_dif_cnt[ch][1] +
                      ctx->sound_var.rar_coefficient[ch][2] * ctx->sound_var.rar_dif_cnt[ch][2] +
                      ctx->sound_var.rar_coefficient[ch][3] * ctx->sound_var.rar_dif_cnt[ch][3]) >>
                     3);
}

static int ace_sound_calc_tabs(ace_decompress_ctx_t *ctx)
{
    int i;
    for(i = 0; i < ctx->sound_var.models; i++)
    {
        ace_read_widths(ctx, ACE_SOUND_MAX_CODE_WIDTH, ctx->sound_huff_symbols[i], ctx->sound_huff_widths[i],
                        ACE_SOUND_MAX_CODE);
    }
    ctx->sound_var.sound_block_size = ctx->read_code >> (32 - 15);
    ace_add_bits(ctx, 15);
    return 1;
}

static int ace_sound_get_symbol(ace_decompress_ctx_t *ctx, int model, int channel)
{
    int symbol;

    if(!ctx->sound_var.sound_block_size) ace_sound_calc_tabs(ctx);

    model <<= 1;
    if(!model) model += ctx->sound_var.adaptive_model_use[channel];
    model += 3 * channel;

    symbol = ctx->sound_huff_symbols[model][ctx->read_code >> (32 - ACE_SOUND_MAX_CODE_WIDTH)];
    ace_add_bits(ctx, ctx->sound_huff_widths[model][symbol]);
    ctx->sound_var.sound_block_size--;

    return symbol;
}

static int ace_sound_get(ace_decompress_ctx_t *ctx, int channel)
{
    int err;

    if(ctx->sound_var.state[channel] != 2)
    {
        ctx->sound_var.code[channel] = ace_sound_get_symbol(ctx, ctx->sound_var.state[channel], channel);

        if(ctx->sound_var.code[channel] == ACE_SOUND_TYPE_CODE)
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
            return -1;
        }
    }

    if(ctx->sound_var.state[channel] != 2)
    {
        if(!ctx->sound_var.state[channel] && ctx->sound_var.code[channel] < ACE_SOUND_RUNLEN_CODES)
        {
            ctx->sound_var.state[channel] = 2;
        }
        else
        {
            if(ctx->sound_var.state[channel] == 1)
            {
                err                           = ctx->sound_var.code[channel];
                ctx->sound_var.state[channel] = 0;
            }
            else
            {
                err = ctx->sound_var.code[channel] - ACE_SOUND_RUNLEN_CODES;

                ctx->sound_var.adaptive_model_cnt[channel] =
                    (ctx->sound_var.adaptive_model_cnt[channel] * 7 >> 3) + err;

                ctx->sound_var.adaptive_model_use[channel] = ctx->sound_var.adaptive_model_cnt[channel] > 40;
            }
        }
    }

    if(ctx->sound_var.state[channel] == 2)
    {
        if(!ctx->sound_var.code[channel])
            ctx->sound_var.state[channel] = 1;
        else
            ctx->sound_var.code[channel]--;

        err = 0;
    }

    return err & 1 ? (255 - (err >> 1)) : err >> 1;
}

static int ace_sound_rar_predict(ace_decompress_ctx_t *ctx, int channel)
{
    int pch = ace_sound_get_predicted(ctx, channel);

    if(ctx->sound_var.predictor_dif_cnt[channel][0] > ctx->sound_var.predictor_dif_cnt[channel][1])
        pch = ctx->sound_var.last_byte[channel];

    return pch - 128;
}

static void ace_sound_rar_adjust(ace_decompress_ctx_t *ctx, int channel, int ch)
{
    int pred_dif, i, min_dif, min_dif_pos = 0, pch;

    ctx->sound_var.byte_count[channel]++;

    pch      = ace_sound_get_predicted(ctx, channel);
    pred_dif = ((int8_t)(pch - ch)) << 3;

    ctx->sound_var.rar_dif[channel][0] += abs(pred_dif - ctx->sound_var.rar_dif_cnt[channel][0]);
    ctx->sound_var.rar_dif[channel][1] += abs(pred_dif + ctx->sound_var.rar_dif_cnt[channel][0]);
    ctx->sound_var.rar_dif[channel][2] += abs(pred_dif - ctx->sound_var.rar_dif_cnt[channel][1]);
    ctx->sound_var.rar_dif[channel][3] += abs(pred_dif + ctx->sound_var.rar_dif_cnt[channel][1]);
    ctx->sound_var.rar_dif[channel][4] += abs(pred_dif - ctx->sound_var.rar_dif_cnt[channel][2]);
    ctx->sound_var.rar_dif[channel][5] += abs(pred_dif + ctx->sound_var.rar_dif_cnt[channel][2]);
    ctx->sound_var.rar_dif[channel][6] += abs(pred_dif - ctx->sound_var.rar_dif_cnt[channel][3]);
    ctx->sound_var.rar_dif[channel][7] += abs(pred_dif + ctx->sound_var.rar_dif_cnt[channel][3]);
    ctx->sound_var.rar_dif[channel][8] += abs(pred_dif);

    ctx->sound_var.predictor_dif_cnt[channel][0] += ctx->sound_quantizer[(uint8_t)(pred_dif >> 3)];
    ctx->sound_var.predictor_dif_cnt[channel][1] +=
        ctx->sound_quantizer[(uint8_t)(ctx->sound_var.last_byte[channel] - ch)];

    ctx->sound_var.last_delta[channel] = (int8_t)(ch - ctx->sound_var.last_byte[channel]);
    ctx->sound_var.last_byte[channel]  = ch;

    if(!(ctx->sound_var.byte_count[channel] & 0x1f))
    {
        min_dif = 0xffff;
        for(i = 8; i >= 0; i--)
        {
            if(ctx->sound_var.rar_dif[channel][i] <= min_dif)
            {
                min_dif     = ctx->sound_var.rar_dif[channel][i];
                min_dif_pos = i;
            }
            ctx->sound_var.rar_dif[channel][i] = 0;
        }

        if(min_dif_pos != 8)
        {
            i = min_dif_pos >> 1;
            if(!(min_dif_pos & 1))
            {
                if(ctx->sound_var.rar_coefficient[channel][i] >= -16) ctx->sound_var.rar_coefficient[channel][i]--;
            }
            else
            {
                if(ctx->sound_var.rar_coefficient[channel][i] <= 16) ctx->sound_var.rar_coefficient[channel][i]++;
            }
        }

        if(!(ctx->sound_var.byte_count[channel] & 0xff))
        {
            ctx->sound_var.predictor_dif_cnt[channel][0] -= ctx->sound_var.last_predictor_dif_cnt[channel][0];
            ctx->sound_var.last_predictor_dif_cnt[channel][0] = ctx->sound_var.predictor_dif_cnt[channel][0];
            ctx->sound_var.predictor_dif_cnt[channel][1] -= ctx->sound_var.last_predictor_dif_cnt[channel][1];
            ctx->sound_var.last_predictor_dif_cnt[channel][1] = ctx->sound_var.predictor_dif_cnt[channel][1];
        }
    }

    ctx->sound_var.rar_dif_cnt[channel][3] = ctx->sound_var.rar_dif_cnt[channel][2];
    ctx->sound_var.rar_dif_cnt[channel][2] = ctx->sound_var.rar_dif_cnt[channel][1];
    ctx->sound_var.rar_dif_cnt[channel][1] =
        ctx->sound_var.last_delta[channel] - ctx->sound_var.rar_dif_cnt[channel][0];
    ctx->sound_var.rar_dif_cnt[channel][0] = ctx->sound_var.last_delta[channel];
}

int ace_sound_block(ace_decompress_ctx_t *ctx, uint8_t *buf, int len)
{
    int i, err, channel, sample;

    len &= (int)0xfffffffc;
    if(len > (int)ctx->file_size) len = (int)ctx->file_size;

    for(i = 0; i < len; i++)
    {
        channel = ace_sound_channel_num[ctx->sound_var.mode][i & 3];
        err     = ace_sound_get(ctx, channel);

        if(err == -1) break;

        sample = (uint8_t)(err + ace_sound_rar_predict(ctx, channel));
        buf[i] = (uint8_t)(sample + 128);
        ace_sound_rar_adjust(ctx, channel, buf[i]);
    }

    ctx->file_size -= i;
    return i;
}

void ace_sound_init(ace_decompress_ctx_t *ctx, int type)
{
    memset(&ctx->sound_var, 0, sizeof(ctx->sound_var));
    ctx->sound_var.mode   = type - ACE_BLOCK_SOUND_8;
    ctx->sound_var.models = ace_sound_models[ctx->sound_var.mode];
}
