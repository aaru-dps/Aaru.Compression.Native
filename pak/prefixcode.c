/*
 * prefixcode.c - Prefix code tree implementation
 *
 * Copyright (c) 2017-pstatic inline bool is_invalid_node(PrefixCode *self, int node) {
    (void)self; // Suppress unused parameter warning
    return (node < 0);
}ent, MacPaw Inc. All rights reserved.
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

#include "prefixcode.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

// Safe realloc that frees original pointer on failure
static void *safe_realloc(void *ptr, size_t newsize)
{
    void *newptr = realloc(ptr, newsize);
    if(!newptr && newsize > 0)
    {
        free(ptr);
        return NULL;
    }
    return newptr;
}

// Inline helper functions
static inline CodeTreeNode *node_pointer(PrefixCode *self, int node) { return &self->tree[node]; }

static inline int branch(PrefixCode *self, int node, int bit) { return node_pointer(self, node)->branches[bit]; }

static inline void set_branch(PrefixCode *self, int node, int bit, int nextnode)
{
    node_pointer(self, node)->branches[bit] = nextnode;
}

static inline int left_branch(PrefixCode *self, int node) { return branch(self, node, 0); }

static inline int right_branch(PrefixCode *self, int node) { return branch(self, node, 1); }

static inline void set_left_branch(PrefixCode *self, int node, int nextnode) { set_branch(self, node, 0, nextnode); }

static inline void set_right_branch(PrefixCode *self, int node, int nextnode) { set_branch(self, node, 1, nextnode); }

static inline int leaf_value(PrefixCode *self, int node) { return left_branch(self, node); }

static inline void set_leaf_value(PrefixCode *self, int node, int value)
{
    set_left_branch(self, node, value);
    set_right_branch(self, node, value);
}

static inline void set_empty_node(PrefixCode *self, int node)
{
    set_left_branch(self, node, -1);
    set_right_branch(self, node, -2);
}

static inline bool is_invalid_node(PrefixCode *self, int node) { return node < 0; }

static inline bool is_open_branch(PrefixCode *self, int node, int bit)
{
    return is_invalid_node(self, branch(self, node, bit));
}

static inline bool is_empty_node(PrefixCode *self, int node)
{
    return left_branch(self, node) == -1 && right_branch(self, node) == -2;
}

static inline bool is_leaf_node(PrefixCode *self, int node)
{
    return left_branch(self, node) == right_branch(self, node);
}

static int new_node(PrefixCode *self)
{
    CodeTreeNode *newtree = safe_realloc(self->tree, (self->numentries + 1) * sizeof(CodeTreeNode));
    if(!newtree) return -1;

    self->tree = newtree;
    set_empty_node(self, self->numentries);
    return self->numentries++;
}

// Stack implementation for tree building
static PrefixCodeStack *prefix_code_stack_alloc(void)
{
    PrefixCodeStack *stack = malloc(sizeof(PrefixCodeStack));
    if(!stack) return NULL;

    stack->data = malloc(16 * sizeof(int));
    if(!stack->data)
    {
        free(stack);
        return NULL;
    }

    stack->count    = 0;
    stack->capacity = 16;
    return stack;
}

static void prefix_code_stack_free(PrefixCodeStack *stack)
{
    if(!stack) return;
    free(stack->data);
    free(stack);
}

static int prefix_code_stack_push(PrefixCodeStack *stack, int value)
{
    if(stack->count >= stack->capacity)
    {
        int  newcapacity = stack->capacity * 2;
        int *newdata     = safe_realloc(stack->data, newcapacity * sizeof(int));
        if(!newdata) return -1;

        stack->data     = newdata;
        stack->capacity = newcapacity;
    }

    stack->data[stack->count++] = value;
    return 0;
}

static int prefix_code_stack_pop(PrefixCodeStack *stack)
{
    if(stack->count == 0) return -1;
    return stack->data[--stack->count];
}

static void prefix_code_stack_clear(PrefixCodeStack *stack) { stack->count = 0; }

// Bit reversal functions
static uint32_t reverse_32(uint32_t val)
{
    val = ((val >> 1) & 0x55555555) | ((val & 0x55555555) << 1);
    val = ((val >> 2) & 0x33333333) | ((val & 0x33333333) << 2);
    val = ((val >> 4) & 0x0F0F0F0F) | ((val & 0x0F0F0F0F) << 4);
    val = ((val >> 8) & 0x00FF00FF) | ((val & 0x00FF00FF) << 8);
    return (val >> 16) | (val << 16);
}

static uint32_t reverse_n(uint32_t val, int length) { return reverse_32(val) >> (32 - length); }

// Table construction functions
#define TABLE_MAX_SIZE 10

static void make_table(PrefixCode *code, int node, CodeTableEntry *table, int depth, int maxdepth)
{
    int currtablesize = 1 << (maxdepth - depth);

    if(is_invalid_node(code, node))
    {
        for(int i = 0; i < currtablesize; i++) table[i].length = -1;
    }
    else if(is_leaf_node(code, node))
    {
        for(int i = 0; i < currtablesize; i++)
        {
            table[i].length = depth;
            table[i].value  = leaf_value(code, node);
        }
    }
    else
    {
        if(depth == maxdepth)
        {
            table[0].length = maxdepth + 1;
            table[0].value  = node;
        }
        else
        {
            make_table(code, left_branch(code, node), table, depth + 1, maxdepth);
            make_table(code, right_branch(code, node), table + currtablesize / 2, depth + 1, maxdepth);
        }
    }
}

static void make_table_le(PrefixCode *code, int node, CodeTableEntry *table, int depth, int maxdepth)
{
    int currtablesize = 1 << (maxdepth - depth);
    int currstride    = 1 << depth;

    if(is_invalid_node(code, node))
    {
        for(int i = 0; i < currtablesize; i++) table[i * currstride].length = -1;
    }
    else if(is_leaf_node(code, node))
    {
        for(int i = 0; i < currtablesize; i++)
        {
            table[i * currstride].length = depth;
            table[i * currstride].value  = leaf_value(code, node);
        }
    }
    else
    {
        if(depth == maxdepth)
        {
            table[0].length = maxdepth + 1;
            table[0].value  = node;
        }
        else
        {
            make_table_le(code, left_branch(code, node), table, depth + 1, maxdepth);
            make_table_le(code, right_branch(code, node), table + currstride, depth + 1, maxdepth);
        }
    }
}

static int prefix_code_make_table(PrefixCode *self)
{
    if(self->table1) return PREFIX_CODE_OK;

    if(self->maxlength < self->minlength)
        self->tablesize = TABLE_MAX_SIZE;  // no code lengths recorded
    else if(self->maxlength >= TABLE_MAX_SIZE)
        self->tablesize = TABLE_MAX_SIZE;
    else
        self->tablesize = self->maxlength;

    self->table1 = malloc(sizeof(CodeTableEntry) * (1 << self->tablesize));
    if(!self->table1) return PREFIX_CODE_INVALID;

    make_table(self, 0, self->table1, 0, self->tablesize);
    return PREFIX_CODE_OK;
}

static int prefix_code_make_table_le(PrefixCode *self)
{
    if(self->table2) return PREFIX_CODE_OK;

    if(self->maxlength < self->minlength)
        self->tablesize = TABLE_MAX_SIZE;  // no code lengths recorded
    else if(self->maxlength >= TABLE_MAX_SIZE)
        self->tablesize = TABLE_MAX_SIZE;
    else
        self->tablesize = self->maxlength;

    self->table2 = malloc(sizeof(CodeTableEntry) * (1 << self->tablesize));
    if(!self->table2) return PREFIX_CODE_INVALID;

    make_table_le(self, 0, self->table2, 0, self->tablesize);
    return PREFIX_CODE_OK;
}

// Public functions

PrefixCode *prefix_code_alloc(void)
{
    PrefixCode *self = malloc(sizeof(PrefixCode));
    if(!self) return NULL;

    self->tree = malloc(sizeof(CodeTreeNode));
    if(!self->tree)
    {
        free(self);
        return NULL;
    }

    set_empty_node(self, 0);
    self->numentries = 1;
    self->minlength  = INT_MAX;
    self->maxlength  = INT_MIN;
    self->isstatic   = false;

    self->stack  = NULL;
    self->table1 = self->table2 = NULL;
    self->tablesize             = 0;
    self->currnode              = 0;

    return self;
}

PrefixCode *prefix_code_alloc_with_static_table(int (*statictable)[2])
{
    PrefixCode *self = malloc(sizeof(PrefixCode));
    if(!self) return NULL;

    self->tree     = (CodeTreeNode *)statictable;  // TODO: fix the ugly cast
    self->isstatic = true;

    self->stack  = NULL;
    self->table1 = self->table2 = NULL;
    self->tablesize             = 0;
    self->currnode              = 0;
    self->numentries            = 0;
    self->minlength             = INT_MAX;
    self->maxlength             = INT_MIN;

    return self;
}

PrefixCode *prefix_code_alloc_with_lengths(const int *lengths, int numsymbols, int maxcodelength, bool zeros)
{
    PrefixCode *self = prefix_code_alloc();
    if(!self) return NULL;

    int code = 0, symbolsleft = numsymbols;

    for(int length = 1; length <= maxcodelength; length++)
    {
        for(int i = 0; i < numsymbols; i++)
        {
            if(lengths[i] != length) continue;
            // Instead of reversing to get a low-bit-first code, we shift and use high-bit-first.
            int result;
            if(zeros) { result = prefix_code_add_value_high_bit_first(self, i, code, length); }
            else { result = prefix_code_add_value_high_bit_first(self, i, ~code, length); }
            if(result != PREFIX_CODE_OK)
            {
                prefix_code_free(self);
                return NULL;
            }
            code++;
            if(--symbolsleft == 0) return self;  // early exit if all codes have been handled
        }
        code <<= 1;
    }

    return self;
}

void prefix_code_free(PrefixCode *self)
{
    if(!self) return;

    if(!self->isstatic) free(self->tree);
    free(self->table1);
    free(self->table2);
    if(self->stack) prefix_code_stack_free(self->stack);
    free(self);
}

int prefix_code_add_value_high_bit_first(PrefixCode *self, int value, uint32_t code, int length)
{
    return prefix_code_add_value_high_bit_first_repeat(self, value, code, length, length);
}

int prefix_code_add_value_high_bit_first_repeat(PrefixCode *self, int value, uint32_t code, int length, int repeatpos)
{
    if(!self || self->isstatic) return PREFIX_CODE_INVALID;

    free(self->table1);
    free(self->table2);
    self->table1 = self->table2 = NULL;

    if(length > self->maxlength) self->maxlength = length;
    if(length < self->minlength) self->minlength = length;

    repeatpos = length - 1 - repeatpos;
    if(repeatpos == 0 ||
       (repeatpos >= 0 && (((code >> (repeatpos - 1)) & 3) == 0 || ((code >> (repeatpos - 1)) & 3) == 3)))
    {
        return PREFIX_CODE_INVALID;
    }

    int lastnode = 0;
    for(int bitpos = length - 1; bitpos >= 0; bitpos--)
    {
        int bit = (code >> bitpos) & 1;

        if(is_leaf_node(self, lastnode)) return PREFIX_CODE_INVALID;

        if(bitpos == repeatpos)
        {
            if(!is_open_branch(self, lastnode, bit)) return PREFIX_CODE_INVALID;

            int repeatnode = new_node(self);
            int nextnode   = new_node(self);
            if(repeatnode < 0 || nextnode < 0) return PREFIX_CODE_INVALID;

            set_branch(self, lastnode, bit, repeatnode);
            set_branch(self, repeatnode, bit, repeatnode);
            set_branch(self, repeatnode, bit ^ 1, nextnode);
            lastnode = nextnode;

            bitpos++;  // terminating bit already handled, skip it
        }
        else
        {
            if(is_open_branch(self, lastnode, bit))
            {
                int newnode = new_node(self);
                if(newnode < 0) return PREFIX_CODE_INVALID;
                set_branch(self, lastnode, bit, newnode);
            }
            lastnode = branch(self, lastnode, bit);
        }
    }

    if(!is_empty_node(self, lastnode)) return PREFIX_CODE_INVALID;
    set_leaf_value(self, lastnode, value);
    return PREFIX_CODE_OK;
}

int prefix_code_add_value_low_bit_first(PrefixCode *self, int value, uint32_t code, int length)
{
    return prefix_code_add_value_high_bit_first(self, value, reverse_n(code, length), length);
}

int prefix_code_add_value_low_bit_first_repeat(PrefixCode *self, int value, uint32_t code, int length, int repeatpos)
{
    return prefix_code_add_value_high_bit_first_repeat(self, value, reverse_n(code, length), length, repeatpos);
}

void prefix_code_start_building_tree(PrefixCode *self)
{
    if(!self) return;

    self->currnode = 0;
    if(!self->stack) { self->stack = prefix_code_stack_alloc(); }
    else { prefix_code_stack_clear(self->stack); }
}

void prefix_code_start_zero_branch(PrefixCode *self)
{
    if(!self) return;

    int new = new_node(self);
    if(new < 0) return;

    set_branch(self, self->currnode, 0, new);
    prefix_code_stack_push(self->stack, self->currnode);
    self->currnode = new;
}

void prefix_code_start_one_branch(PrefixCode *self)
{
    if(!self) return;

    int new = new_node(self);
    if(new < 0) return;

    set_branch(self, self->currnode, 1, new);
    prefix_code_stack_push(self->stack, self->currnode);
    self->currnode = new;
}

void prefix_code_finish_branches(PrefixCode *self)
{
    if(!self || !self->stack) return;

    int node = prefix_code_stack_pop(self->stack);
    if(node >= 0) self->currnode = node;
}

void prefix_code_make_leaf_with_value(PrefixCode *self, int value)
{
    if(!self) return;

    set_leaf_value(self, self->currnode, value);
    prefix_code_finish_branches(self);
}

// BitStream interface functions

int prefix_code_read_symbol(BitStream *bs, PrefixCode *code)
{
    if(!code) return PREFIX_CODE_INVALID;
    if(!code->table1)
    {
        if(prefix_code_make_table(code) != PREFIX_CODE_OK) return PREFIX_CODE_INVALID;
    }

    int bits = bitstream_peek_bits(bs, code->tablesize);

    int length = code->table1[bits].length;
    int value  = code->table1[bits].value;

    if(length < 0) return PREFIX_CODE_INVALID;

    if(length <= code->tablesize)
    {
        bitstream_skip_bits(bs, length);
        return value;
    }

    bitstream_skip_bits(bs, code->tablesize);

    int node = value;
    while(!is_leaf_node(code, node))
    {
        int bit = bitstream_read_bit(bs);
        if(is_open_branch(code, node, bit)) return PREFIX_CODE_INVALID;
        node = branch(code, node, bit);
    }
    return leaf_value(code, node);
}

int prefix_code_read_symbol_le(BitStream *bs, PrefixCode *code)
{
    if(!code) return PREFIX_CODE_INVALID;
    if(!code->table2)
    {
        if(prefix_code_make_table_le(code) != PREFIX_CODE_OK) return PREFIX_CODE_INVALID;
    }

    int bits = bitstream_peek_bits_le(bs, code->tablesize);

    int length = code->table2[bits].length;
    int value  = code->table2[bits].value;

    if(length < 0) return PREFIX_CODE_INVALID;

    if(length <= code->tablesize)
    {
        bitstream_skip_bits_le(bs, length);
        return value;
    }

    bitstream_skip_bits_le(bs, code->tablesize);

    int node = value;
    while(!is_leaf_node(code, node))
    {
        int bit = bitstream_read_bit_le(bs);
        if(is_open_branch(code, node, bit)) return PREFIX_CODE_INVALID;
        node = branch(code, node, bit);
    }
    return leaf_value(code, node);
}
