/*
 * prefixcode.h - Prefix code tree implementation
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

#ifndef PREFIXCODE_H
#define PREFIXCODE_H

#include <stdbool.h>
#include <stdint.h>
#include "bitstream.h"

// Error codes
#define PREFIX_CODE_OK      0
#define PREFIX_CODE_INVALID -1

typedef struct CodeTreeNode
{
    int branches[2];
} CodeTreeNode;

typedef struct CodeTableEntry
{
    uint32_t length;
    int32_t  value;
} CodeTableEntry;

// Simple stack implementation for tree building
typedef struct PrefixCodeStack
{
    int *data;
    int  count;
    int  capacity;
} PrefixCodeStack;

typedef struct PrefixCode
{
    CodeTreeNode *tree;
    int           numentries;
    int           minlength;
    int           maxlength;
    bool          isstatic;

    int              currnode;
    PrefixCodeStack *stack;

    int             tablesize;
    CodeTableEntry *table1;
    CodeTableEntry *table2;
} PrefixCode;

// Function declarations
PrefixCode *prefix_code_alloc(void);
PrefixCode *prefix_code_alloc_with_lengths(const int *lengths, int numsymbols, int maxlength, bool shortestCodeIsZeros);
PrefixCode *prefix_code_alloc_with_static_table(int (*statictable)[2]);
void        prefix_code_free(PrefixCode *self);

int prefix_code_add_value_high_bit_first(PrefixCode *self, int value, uint32_t code, int length);
int prefix_code_add_value_high_bit_first_repeat(PrefixCode *self, int value, uint32_t code, int length, int repeatpos);
int prefix_code_add_value_low_bit_first(PrefixCode *self, int value, uint32_t code, int length);
int prefix_code_add_value_low_bit_first_repeat(PrefixCode *self, int value, uint32_t code, int length, int repeatpos);

void prefix_code_start_building_tree(PrefixCode *self);
void prefix_code_start_zero_branch(PrefixCode *self);
void prefix_code_start_one_branch(PrefixCode *self);
void prefix_code_finish_branches(PrefixCode *self);
void prefix_code_make_leaf_with_value(PrefixCode *self, int value);

// BitStream interface functions
int prefix_code_read_symbol(BitStream *bs, PrefixCode *code);
int prefix_code_read_symbol_le(BitStream *bs, PrefixCode *code);

#endif /* PREFIXCODE_H */
