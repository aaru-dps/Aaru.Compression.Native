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

#ifndef LZD_H
#define LZD_H

#include <stddef.h>
#include <stdint.h>

#define MAXBITS     13
#define MAXMAX      ((1U << MAXBITS) - 1)   // 8191
#define FIRST_FREE  258                     // first free code after CLEAR+EOF
#define CLEAR       256
#define Z_EOF 257

typedef enum
{
    LZD_OK          = 0,
    LZD_NEED_INPUT  = 1,
    LZD_NEED_OUTPUT = 2,
    LZD_DONE        = 3
} LZDStatus;

typedef struct
{
    int *    head;
    uint8_t *tail;
    unsigned nbits;
    unsigned max_code;
    unsigned free_code;
    unsigned old_code;
    int      have_old;

    char *stack;
    char *stack_lim;
    char *stack_ptr;

    uint64_t bitbuf;
    int      bitcount;

    const unsigned char *in_ptr;
    size_t               in_len;
    size_t               in_pos;
} LZDContext;

static const unsigned masks[MAXBITS + 1] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    (1u << 9) - 1,
    (1u << 10) - 1,
    (1u << 11) - 1,
    (1u << 12) - 1,
    (1u << 13) - 1
};

LZDStatus LZD_Init(LZDContext *ctx);

LZDStatus LZD_Feed(LZDContext *ctx, const unsigned char *in, size_t in_len);

LZDStatus LZD_Drain(LZDContext *ctx, unsigned char *out, size_t out_len, size_t *out_produced);

void LZD_Destroy(LZDContext *ctx);

#endif // LZD_H
