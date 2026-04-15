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

#include "lzss.h"

int rar_lzss_init(rar_lzss_t *lzss, size_t window_size)
{
    /* Ensure power of 2 */
    if(window_size == 0 || (window_size & (window_size - 1)) != 0) return -1;

    lzss->window = (uint8_t *)malloc(window_size);
    if(!lzss->window) return -1;

    lzss->mask     = window_size - 1;
    lzss->position = 0;
    memset(lzss->window, 0, window_size);

    return 0;
}

void rar_lzss_restart(rar_lzss_t *lzss)
{
    lzss->position = 0;
    if(lzss->window) memset(lzss->window, 0, lzss->mask + 1);
}

void rar_lzss_cleanup(rar_lzss_t *lzss)
{
    if(lzss->window)
    {
        free(lzss->window);
        lzss->window = NULL;
    }
    lzss->mask     = 0;
    lzss->position = 0;
}

void rar_lzss_copy_bytes(const rar_lzss_t *lzss, uint8_t *dest, int64_t start, size_t length)
{
    size_t window_size = lzss->mask + 1;
    size_t win_start   = (size_t)start & lzss->mask;

    if(win_start + length <= window_size)
    {
        /* No wrap-around */
        memcpy(dest, &lzss->window[win_start], length);
    }
    else
    {
        /* Spans window boundary */
        size_t first = window_size - win_start;
        memcpy(dest, &lzss->window[win_start], first);
        memcpy(dest + first, &lzss->window[0], length - first);
    }
}
