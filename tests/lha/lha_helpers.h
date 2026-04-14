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

#ifndef AARU_COMPRESSION_NATIVE__TESTS_LHA_HELPERS_H_
#define AARU_COMPRESSION_NATIVE__TESTS_LHA_HELPERS_H_

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/*
 * Minimal LHA archive header parser for tests.
 * Extracts compressed payload from an LHA/LArc/PMarc archive file.
 * Supports header levels 0, 1, and 2.
 */
struct lha_test_info
{
    char     method[6];         /* e.g., "-lh6-" */
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint8_t  level;
    size_t   payload_offset;    /* offset of compressed data in the file */
    size_t   payload_size;      /* actual compressed data size */
};

static int parse_lha_header(const uint8_t *data, size_t data_len, struct lha_test_info *info)
{
    if(data_len < 22) return -1;

    uint8_t  header_size_byte = data[0];
    uint32_t compressed_size  = (uint32_t)data[7] | ((uint32_t)data[8] << 8) | ((uint32_t)data[9] << 16) |
                               ((uint32_t)data[10] << 24);
    uint32_t uncompressed_size = (uint32_t)data[11] | ((uint32_t)data[12] << 8) | ((uint32_t)data[13] << 16) |
                                ((uint32_t)data[14] << 24);
    uint8_t level = data[20];

    memcpy(info->method, &data[2], 5);
    info->method[5]          = '\0';
    info->compressed_size    = compressed_size;
    info->uncompressed_size  = uncompressed_size;
    info->level              = level;

    if(level == 0 || level == 1)
    {
        size_t base_header = (size_t)header_size_byte + 2;
        info->payload_offset = base_header;
        info->payload_size   = compressed_size;
    }
    else if(level == 2)
    {
        uint16_t header_size = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
        info->payload_offset = header_size;
        info->payload_size   = compressed_size;
    }
    else
    {
        return -1;
    }

    return 0;
}

static uint8_t *load_lha_payload(const char *filename, struct lha_test_info *info)
{
    FILE   *file;
    size_t  file_size;
    uint8_t *file_data;
    uint8_t *payload;

    file = fopen(filename, "rb");
    if(!file) return NULL;

    fseek(file, 0, SEEK_END);
    file_size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    file_data = (uint8_t *)malloc(file_size);
    if(!file_data) { fclose(file); return NULL; }

    fread(file_data, 1, file_size, file);
    fclose(file);

    if(parse_lha_header(file_data, file_size, info) != 0)
    {
        free(file_data);
        return NULL;
    }

    if(info->payload_offset + info->payload_size > file_size)
    {
        free(file_data);
        return NULL;
    }

    payload = (uint8_t *)malloc(info->payload_size);
    if(!payload) { free(file_data); return NULL; }

    memcpy(payload, file_data + info->payload_offset, info->payload_size);
    free(file_data);

    return payload;
}

#endif /* AARU_COMPRESSION_NATIVE__TESTS_LHA_HELPERS_H_ */
