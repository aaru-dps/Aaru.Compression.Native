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

#include <stdint.h>
#include <string.h>
#include "../library.h"

#define DLE 0x90  // Data Link Escape character, used as a repeat marker.

// Decompresses data using non-repeat packing.
// This algorithm encodes runs of identical bytes.
AARU_EXPORT int AARU_CALL arc_decompress_pack(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf, size_t *out_len)
{
    // Basic validation of pointers.
    if(!in_buf || !out_buf || !out_len) { return -1; }

    size_t        in_pos  = 0;
    size_t        out_pos = 0;
    unsigned char state   = 0;  // 0 for normal (NOHIST), 1 for in-repeat (INREP).
    unsigned char lastc   = 0;  // Last character seen.

    // Loop through the input buffer until it's exhausted or the output buffer is full.
    while(in_pos < in_len && out_pos < *out_len)
    {
        if(state == 1)
        {  // We are in a repeat sequence.
            if(in_buf[in_pos])
            {  // The byte after DLE is the repeat count.
                unsigned char count = in_buf[in_pos];
                // Write the last character 'count' times.
                while(--count && out_pos < *out_len) { out_buf[out_pos++] = lastc; }
            }
            else
            {  // A count of 0 means the DLE character itself should be written.
                if(out_pos < *out_len) { out_buf[out_pos++] = DLE; }
            }
            state = 0;  // Return to normal state.
            in_pos++;
        }
        else
        {  // Normal state.
            if(in_buf[in_pos] != DLE)
            {  // Not a repeat sequence.
                if(out_pos < *out_len)
                {
                    // Copy the character and save it as the last character.
                    out_buf[out_pos++] = lastc = in_buf[in_pos];
                }
            }
            else
            {               // DLE marks the start of a repeat sequence.
                state = 1;  // Enter repeat state.
            }
            in_pos++;
        }
    }

    // Update the output length to the number of bytes written.
    *out_len = out_pos;
    // Return success.
    return 0;
}
