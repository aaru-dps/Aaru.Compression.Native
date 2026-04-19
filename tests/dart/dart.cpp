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

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../library.h"
#include "../crc32.h"
#include "dart_helpers.h"
#include "gtest/gtest.h"

typedef struct
{
    uint8_t      *data;
    size_t        total_size;
    dart_header_t header;
    int16_t       block_lengths[DART_BLOCK_ARRAY_LEN_HIGH];
    int           num_blocks;
    uint8_t      *raw;
    size_t        raw_size;
    size_t        data_offset;
} dart_image_t;

static size_t read_file_to_buffer(const char *path, uint8_t **out)
{
    FILE *f = fopen(path, "rb");
    if(!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(sz <= 0)
    {
        fclose(f);
        return 0;
    }
    *out = (uint8_t *)malloc(sz);
    if(!*out)
    {
        fclose(f);
        return 0;
    }
    size_t rd = fread(*out, 1, sz, f);
    fclose(f);
    if(rd != (size_t)sz)
    {
        free(*out);
        *out = NULL;
        return 0;
    }
    return (size_t)sz;
}

static size_t lzip_decompress_file(const char *path, uint8_t **out)
{
    uint8_t *lz_buf  = NULL;
    size_t   lz_size = read_file_to_buffer(path, &lz_buf);
    if(lz_size == 0) return 0;

    size_t try_sizes[] = {512 * 1024, 1024 * 1024, 2 * 1024 * 1024};
    for(int i = 0; i < 3; i++)
    {
        *out           = (uint8_t *)malloc(try_sizes[i]);
        int32_t result = AARU_lzip_decode_buffer(*out, (int32_t)try_sizes[i], lz_buf, (int32_t)lz_size);
        if(result > 0)
        {
            free(lz_buf);
            return (size_t)result;
        }
        free(*out);
        *out = NULL;
    }
    free(lz_buf);
    return 0;
}

static bool load_dart_image(const char *path, dart_image_t *img)
{
    memset(img, 0, sizeof(*img));

    img->raw_size = lzip_decompress_file(path, &img->raw);
    if(img->raw_size == 0) return false;

    img->data_offset = dart_parse_header(img->raw, img->raw_size, &img->header, img->block_lengths, &img->num_blocks);
    if(img->data_offset == 0)
    {
        free(img->raw);
        img->raw = NULL;
        return false;
    }

    int active_blocks = 0;
    for(int i = 0; i < img->num_blocks; i++)
    {
        if(img->block_lengths[i] != 0) active_blocks++;
    }
    img->total_size = active_blocks * DART_BUFFER_SIZE;

    if(img->header.srcCmp == DART_COMPRESS_RLE)
    {
        img->data      = (uint8_t *)malloc(img->total_size);
        size_t out_pos = 0;
        size_t in_pos  = img->data_offset;

        for(int i = 0; i < img->num_blocks; i++)
        {
            if(img->block_lengths[i] == 0) continue;

            if(img->block_lengths[i] == -1)
            {
                if(in_pos + DART_BUFFER_SIZE <= img->raw_size)
                    memcpy(img->data + out_pos, img->raw + in_pos, DART_BUFFER_SIZE);
                in_pos += DART_BUFFER_SIZE;
            }
            else
            {
                size_t comp_size = (size_t)img->block_lengths[i] * 2;
                if(in_pos + comp_size <= img->raw_size)
                {
                    AARU_apple_rle_decode_buffer(img->data + out_pos, DART_BUFFER_SIZE, img->raw + in_pos,
                                                 (int32_t)comp_size);
                }
                in_pos += comp_size;
            }
            out_pos += DART_BUFFER_SIZE;
        }
    }

    return true;
}

static void free_dart_image(dart_image_t *img)
{
    if(img->data) free(img->data);
    if(img->raw) free(img->raw);
    memset(img, 0, sizeof(*img));
}

static bool verify_dart_pair(const char *fast_name, const char *best_name)
{
    char path[PATH_MAX];
    char fast_path[PATH_MAX];
    char best_path[PATH_MAX];

    getcwd(path, PATH_MAX);
    snprintf(fast_path, PATH_MAX, "%s/data/%s", path, fast_name);
    snprintf(best_path, PATH_MAX, "%s/data/%s", path, best_name);

    dart_image_t fast_img, best_img;

    if(!load_dart_image(fast_path, &fast_img))
    {
        printf("  SKIP: cannot load %s\n", fast_name);
        return false;
    }
    if(!load_dart_image(best_path, &best_img))
    {
        printf("  SKIP: cannot load %s\n", best_name);
        free_dart_image(&fast_img);
        return false;
    }

    if(fast_img.header.srcCmp != DART_COMPRESS_RLE || best_img.header.srcCmp != DART_COMPRESS_LZH)
    {
        printf("  SKIP: unexpected compression types\n");
        free_dart_image(&fast_img);
        free_dart_image(&best_img);
        return false;
    }

    size_t in_pos        = best_img.data_offset;
    size_t ref_block_idx = 0;
    int    blocks_ok     = 0;
    int    blocks_fail   = 0;

    for(int i = 0; i < best_img.num_blocks; i++)
    {
        if(best_img.block_lengths[i] == 0)
        {
            if(fast_img.block_lengths[i] != 0) ref_block_idx++;
            continue;
        }

        const uint8_t *reference = fast_img.data + ref_block_idx * DART_BUFFER_SIZE;

        if(best_img.block_lengths[i] == -1)
        {
            if(memcmp(best_img.raw + in_pos, reference, DART_BUFFER_SIZE) == 0)
                blocks_ok++;
            else
                blocks_fail++;
            in_pos += DART_BUFFER_SIZE;
        }
        else
        {
            size_t  comp_size = (size_t)best_img.block_lengths[i];
            uint8_t out[DART_BUFFER_SIZE];
            size_t  out_len = DART_BUFFER_SIZE;

            memset(out, 0, DART_BUFFER_SIZE);

            int ret = AARU_apple_lzh_decode_buffer(out, &out_len, best_img.raw + in_pos, comp_size);
            if(ret == 0 && out_len == DART_BUFFER_SIZE && memcmp(out, reference, DART_BUFFER_SIZE) == 0)
                blocks_ok++;
            else
                blocks_fail++;
            in_pos += comp_size;
        }
        ref_block_idx++;
    }

    printf("  %s: %d OK, %d FAIL\n", best_name, blocks_ok, blocks_fail);

    free_dart_image(&fast_img);
    free_dart_image(&best_img);

    return blocks_fail == 0 && blocks_ok > 0;
}

TEST(DartAppleLzh, mf2dd_hfs) { EXPECT_TRUE(verify_dart_pair("mf2dd_hfs_fast.dart.lz", "mf2dd_hfs_best.dart.lz")); }

TEST(DartAppleLzh, mf2dd_mfs) { EXPECT_TRUE(verify_dart_pair("mf2dd_mfs_fast.dart.lz", "mf2dd_mfs_best.dart.lz")); }

TEST(DartAppleLzh, mf1dd_hfs) { EXPECT_TRUE(verify_dart_pair("mf1dd_hfs_fast.dart.lz", "mf1dd_hfs_best.dart.lz")); }

TEST(DartAppleLzh, mf1dd_mfs) { EXPECT_TRUE(verify_dart_pair("mf1dd_mfs_fast.dart.lz", "mf1dd_mfs_best.dart.lz")); }
