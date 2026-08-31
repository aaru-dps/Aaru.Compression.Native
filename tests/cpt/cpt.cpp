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

#include <unistd.h>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../library.h"
#include "../crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32    0x66007DBA
#define EXPECTED_ORIGSIZE 152089

/* ---- RLE only (STORE.CPT data fork) ---- */

#define RLE_COMPRESSED_SIZE 150125

static const uint8_t *rle_buffer;

class CptRleFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/cpt_rle.bin", path);
        FILE *file = fopen(filename, "rb");
        rle_buffer = (const uint8_t *)malloc(RLE_COMPRESSED_SIZE);
        fread((void *)rle_buffer, 1, RLE_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)rle_buffer); }
};

TEST_F(CptRleFixture, cpt_rle)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_cpt_rle_decode_buffer(rle_buffer, RLE_COMPRESSED_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- LZH + RLE (DEFAULTS.CPT data fork) ---- */

#define LZH_RLE_COMPRESSED_SIZE 61101

static const uint8_t *lzh_rle_buffer;

class CptLzhRleFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/cpt_lzh_rle.bin", path);
        FILE *file     = fopen(filename, "rb");
        lzh_rle_buffer = (const uint8_t *)malloc(LZH_RLE_COMPRESSED_SIZE);
        fread((void *)lzh_rle_buffer, 1, LZH_RLE_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)lzh_rle_buffer); }
};

TEST_F(CptLzhRleFixture, cpt_lzh_rle)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_cpt_lzh_rle_decode_buffer(lzh_rle_buffer, LZH_RLE_COMPRESSED_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}
