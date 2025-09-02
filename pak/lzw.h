/*
 * lzw.h - LZW decompression implementation
 *
 * Copyright (c) 2017-present, MacPaw Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */

#ifndef LZW_H
#define LZW_H

#include <stdbool.h>
#include <stdint.h>

#define LZW_NO_ERROR             0
#define LZW_INVALID_CODE_ERROR   1
#define LZW_TOO_MANY_CODES_ERROR 2

typedef struct LZWTreeNode
{
    uint8_t chr;
    int     parent;
} LZWTreeNode;

typedef struct LZW
{
    int numsymbols;
    int maxsymbols;
    int reservedsymbols;
    int prevsymbol;
    int symbolsize;

    uint8_t *buffer;
    int      buffersize;

    LZWTreeNode nodes[];  // Flexible array member (C99)
} LZW;

// Allocate LZW structure
LZW *lzw_alloc(int maxsymbols, int reservedsymbols);

// Free LZW structure
void lzw_free(LZW *self);

// Clear/reset LZW table
void lzw_clear_table(LZW *self);

// Process next symbol
int lzw_next_symbol(LZW *self, int symbol);

// Replace a symbol
int lzw_replace_symbol(LZW *self, int oldsymbol, int symbol);

// Get output length
int lzw_output_length(LZW *self);

// Output to buffer (normal order)
int lzw_output_to_buffer(LZW *self, uint8_t *buffer);

// Output to buffer (reverse order)
int lzw_reverse_output_to_buffer(LZW *self, uint8_t *buffer);

// Inline helper functions
static inline int lzw_symbol_count(LZW *self) { return self->numsymbols; }

static inline bool lzw_symbol_list_full(LZW *self) { return self->numsymbols == self->maxsymbols; }

static inline LZWTreeNode *lzw_symbols(LZW *self) { return self->nodes; }

#endif /* LZW_H */
