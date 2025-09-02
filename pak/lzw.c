/*
 * lzw.c - LZW decompression implementation
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

#include "lzw.h"
#include <stdlib.h>

LZW *lzw_alloc(int maxsymbols, int reservedsymbols)
{
    LZW *self = (LZW *)malloc(sizeof(LZW) + sizeof(LZWTreeNode) * maxsymbols);
    if(!self) return NULL;

    if(maxsymbols < 256 + reservedsymbols)
    {
        free(self);
        return NULL;
    }

    self->maxsymbols      = maxsymbols;
    self->reservedsymbols = reservedsymbols;

    self->buffer     = NULL;
    self->buffersize = 0;

    for(int i = 0; i < 256; i++)
    {
        self->nodes[i].chr    = i;
        self->nodes[i].parent = -1;
    }

    lzw_clear_table(self);

    return self;
}

void lzw_free(LZW *self)
{
    if(self)
    {
        free(self->buffer);
        free(self);
    }
}

void lzw_clear_table(LZW *self)
{
    self->numsymbols = 256 + self->reservedsymbols;
    self->prevsymbol = -1;
    self->symbolsize = 9;  // TODO: technically this depends on reservedsymbols
}

static uint8_t find_first_byte(LZWTreeNode *nodes, int symbol)
{
    while(nodes[symbol].parent >= 0) symbol = nodes[symbol].parent;
    return nodes[symbol].chr;
}

int lzw_next_symbol(LZW *self, int symbol)
{
    if(self->prevsymbol < 0)
    {
        if(symbol >= self->numsymbols) return LZW_INVALID_CODE_ERROR;
        self->prevsymbol = symbol;
        return LZW_NO_ERROR;
    }

    int postfixbyte;
    if(symbol < self->numsymbols) { postfixbyte = find_first_byte(self->nodes, symbol); }
    else if(symbol == self->numsymbols) { postfixbyte = find_first_byte(self->nodes, self->prevsymbol); }
    else { return LZW_INVALID_CODE_ERROR; }

    int parent       = self->prevsymbol;
    self->prevsymbol = symbol;

    if(!lzw_symbol_list_full(self))
    {
        self->nodes[self->numsymbols].parent = parent;
        self->nodes[self->numsymbols].chr    = postfixbyte;
        self->numsymbols++;

        if(!lzw_symbol_list_full(self))
        {
            if((self->numsymbols & (self->numsymbols - 1)) == 0) { self->symbolsize++; }
        }

        return LZW_NO_ERROR;
    }
    else { return LZW_TOO_MANY_CODES_ERROR; }
}

int lzw_replace_symbol(LZW *self, int oldsymbol, int symbol)
{
    if(symbol >= self->numsymbols) return LZW_INVALID_CODE_ERROR;

    self->nodes[oldsymbol].parent = self->prevsymbol;
    self->nodes[oldsymbol].chr    = find_first_byte(self->nodes, symbol);

    self->prevsymbol = symbol;

    return LZW_NO_ERROR;
}

int lzw_output_length(LZW *self)
{
    int symbol = self->prevsymbol;
    int n      = 0;

    while(symbol >= 0)
    {
        symbol = self->nodes[symbol].parent;
        n++;
    }

    return n;
}

int lzw_output_to_buffer(LZW *self, uint8_t *buffer)
{
    int symbol = self->prevsymbol;
    int n      = lzw_output_length(self);
    buffer += n;

    while(symbol >= 0)
    {
        *--buffer = self->nodes[symbol].chr;
        symbol    = self->nodes[symbol].parent;
    }

    return n;
}

int lzw_reverse_output_to_buffer(LZW *self, uint8_t *buffer)
{
    int symbol = self->prevsymbol;
    int n      = 0;

    while(symbol >= 0)
    {
        *buffer++ = self->nodes[symbol].chr;
        symbol    = self->nodes[symbol].parent;
        n++;
    }

    return n;
}
