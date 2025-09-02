/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2025 Natalia Portillo.
 * Copyright © 2018-2019 David Ryskalczyk
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../library.h"

#define FALSE   0
#define TRUE    !FALSE
#define TABSIZE 4096    // Size of the string table.
#define NO_PRED 0xFFFF  // Indicates no predecessor in the string table.
#define EMPTY   0xFFFF  // Indicates an empty stack.

typedef unsigned char  u_char;
typedef unsigned short u_short;

// Entry in the string table.
struct entry
{
    char    used;         // Is this entry in use?
    u_char  follower;     // The character that follows the string.
    u_short next;         // Next entry in a collision chain.
    u_short predecessor;  // Code for the preceding string.
};

// Static variables for decompression state.
static struct entry *string_tab;
static u_char       *stack;
static int           sp;

// Buffer management variables.
static const u_char *in_buf_ptr;
static size_t        in_len_rem;
static int           inflag;

// Pointer to the hash function to use.
static u_short (*h)(u_short, u_char);

// Original hash function from ARC.
static u_short oldh(u_short pred, u_char foll)
{
    long local;
    local = ((pred + foll) | 0x0800) & 0xFFFF;
    local *= local;
    return (local >> 6) & 0x0FFF;
}

// Newer, faster hash function.
static u_short newh(u_short pred, u_char foll) { return (((pred + foll) & 0xFFFF) * 15073) & 0xFFF; }

// Finds the end of a collision list.
static u_short eolist(u_short index)
{
    int temp;
    while((temp = string_tab[index].next)) index = temp;
    return index;
}

// Hashes a string to find its position in the table.
static u_short hash_it(u_short pred, u_char foll)
{
    u_short       local, tempnext;
    struct entry *ep;

    local = (*h)(pred, foll);

    if(!string_tab[local].used)
        return local;
    else
    {
        local    = eolist(local);
        tempnext = (local + 101) & 0x0FFF;
        ep       = &string_tab[tempnext];

        while(ep->used)
        {
            if(++tempnext == TABSIZE)
            {
                tempnext = 0;
                ep       = string_tab;
            }
            else
                ++ep;
        }
        string_tab[local].next = tempnext;
        return tempnext;
    }
}

// Adds a new string to the table.
static void upd_tab(u_short pred, u_short foll)
{
    struct entry *ep;
    ep              = &string_tab[hash_it(pred, foll)];
    ep->used        = TRUE;
    ep->next        = 0;
    ep->predecessor = pred;
    ep->follower    = foll;
}

// Initializes the string table.
static void init_tab()
{
    memset((char *)string_tab, 0, TABSIZE * sizeof(struct entry));
    for(unsigned int i = 0; i < 256; i++) upd_tab(NO_PRED, i);
}

// Reads a 12-bit code from the input buffer.
static int get_code()
{
    int code;
    if(in_len_rem < 2) return -1;

    if((inflag ^= 1))
    {
        code = (*in_buf_ptr++ << 4);
        code |= (*in_buf_ptr >> 4);
        in_len_rem--;
    }
    else
    {
        code = (*in_buf_ptr++ & 0x0f) << 8;
        code |= (*in_buf_ptr++);
        in_len_rem -= 2;
    }
    return code;
}

// Pushes a character onto the stack.
#define PUSH(c)                        \
    do {                               \
        stack[sp] = ((char)(c));       \
        if(++sp >= TABSIZE) return -1; \
    } while(0)

// Pops a character from the stack.
#define POP() ((sp > 0) ? (int)stack[--sp] : EMPTY)

// Internal crunch decompression logic.
static int arc_decompress_crunch_internal(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                          size_t *out_len, int new_hash)
{
    // Basic validation of pointers.
    if(!in_buf || !out_buf || !out_len) { return -1; }

    // Allocate memory for tables.
    string_tab = (struct entry *)malloc(TABSIZE * sizeof(struct entry));
    stack      = (u_char *)malloc(TABSIZE * sizeof(u_char));
    if(!string_tab || !stack)
    {
        if(string_tab) free(string_tab);
        if(stack) free(stack);
        return -1;
    }

    // Select the hash function.
    if(new_hash)
        h = newh;
    else
        h = oldh;

    // Initialize state.
    sp = 0;
    init_tab();
    int code_count = TABSIZE - 256;
    in_buf_ptr     = in_buf;
    in_len_rem     = in_len;
    inflag         = 0;

    // Main decompression loop.
    int oldcode = get_code();
    if(oldcode == -1)
    {
        *out_len = 0;
        free(string_tab);
        free(stack);
        return 0;
    }
    int finchar = string_tab[oldcode].follower;

    size_t out_pos = 0;
    if(out_pos < *out_len) { out_buf[out_pos++] = finchar; }

    int newcode;
    while((newcode = get_code()) != -1)
    {
        int           code = newcode;
        struct entry *ep   = &string_tab[code];

        // Handle unknown codes and KwKwK case.
        if(!ep->used)
        {
            code = oldcode;
            ep   = &string_tab[code];
            PUSH(finchar);
        }
        // Decode the string by traversing the table.
        while(ep->predecessor != NO_PRED)
        {
            PUSH(ep->follower);
            code = ep->predecessor;
            ep   = &string_tab[code];
        }
        PUSH(finchar = ep->follower);

        // Add the new string to the table if there's room.
        if(code_count)
        {
            upd_tab(oldcode, finchar);
            --code_count;
        }
        oldcode = newcode;

        // Write the decoded string to the output buffer.
        while(sp > 0)
        {
            int c = POP();
            if(c == EMPTY) break;
            if(out_pos < *out_len) { out_buf[out_pos++] = (unsigned char)c; }
        }
    }

    // Clean up and return.
    *out_len = out_pos;
    free(string_tab);
    free(stack);
    return 0;
}

// Decompresses crunched data.
AARU_EXPORT int AARU_CALL arc_decompress_crunch(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                size_t *out_len)
{
    return arc_decompress_crunch_internal(in_buf, in_len, out_buf, out_len, 0);
}

// Decompresses crunched data with non-repeat packing.
AARU_EXPORT int AARU_CALL arc_decompress_crunch_nrpack(const unsigned char *in_buf, size_t in_len,
                                                       unsigned char *out_buf, size_t *out_len)
{
    // Allocate a temporary buffer for the intermediate decompressed data.
    size_t         temp_len = *out_len * 2;  // Heuristic for temp buffer size.
    unsigned char *temp_buf = malloc(temp_len);
    if(!temp_buf) return -1;

    // First, decompress the crunched data.
    int result = arc_decompress_crunch_internal(in_buf, in_len, temp_buf, &temp_len, 0);
    if(result == 0)
    {
        // Then, decompress the non-repeat packing.
        result = arc_decompress_pack(temp_buf, temp_len, out_buf, out_len);
    }

    free(temp_buf);
    return result;
}

// Decompresses crunched data with non-repeat packing and the new hash function.
AARU_EXPORT int AARU_CALL arc_decompress_crunch_nrpack_new(const unsigned char *in_buf, size_t in_len,
                                                           unsigned char *out_buf, size_t *out_len)
{
    // Allocate a temporary buffer.
    size_t         temp_len = *out_len * 2;  // Heuristic.
    unsigned char *temp_buf = malloc(temp_len);
    if(!temp_buf) return -1;

    // Decompress crunched data with the new hash.
    int result = arc_decompress_crunch_internal(in_buf, in_len, temp_buf, &temp_len, 1);
    if(result == 0)
    {
        // Decompress non-repeat packing.
        result = arc_decompress_pack(temp_buf, temp_len, out_buf, out_len);
    }

    free(temp_buf);
    return result;
}
