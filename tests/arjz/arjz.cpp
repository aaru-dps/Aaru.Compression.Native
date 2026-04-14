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
#include <unistd.h>

#include "../../library.h"
#include "../crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32    0x66007DBA
#define EXPECTED_ORIGSIZE 152089

static const uint8_t *buffer_arjz;

class arjz_defaultFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/arjz_default.bin", path);
        FILE *file = fopen(filename, "rb");
        buffer_arjz = (const uint8_t *)malloc(52577);
        fread((void *)buffer_arjz, 1, 52577, file);
        fclose(file);
    }
    void TearDown() { free((void *)buffer_arjz); }
};

TEST_F(arjz_defaultFixture, arjz_default)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = arjz_decompress_method1(buffer_arjz, 52577, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- ARJZ v55 (custom extended DEFLATE) ---- */

static const uint8_t *buffer_arjz_v55;

class arjz_v55Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/arjz_v55_new.bin", path);
        FILE *file = fopen(filename, "rb");
        buffer_arjz_v55 = (const uint8_t *)malloc(52010);
        fread((void *)buffer_arjz_v55, 1, 52010, file);
        fclose(file);
    }
    void TearDown() { free((void *)buffer_arjz_v55); }
};

TEST_F(arjz_v55Fixture, arjz_v55_deflatez)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = arjz_decompress_buffer(buffer_arjz_v55, 52010, outBuf, &destLen, EXPECTED_ORIGSIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}
