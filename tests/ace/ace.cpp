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

#include "../../library.h"
#include "../crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0x66007dba

static const uint8_t *buffer_v1;
static const uint8_t *buffer_v2;

class ace_v1Fixture : public ::testing::Test
{
public:
    ace_v1Fixture() {}

protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];

        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/ace_v1_lz77.bin", path);

        FILE *file = fopen(filename, "rb");
        buffer_v1  = (const uint8_t *)malloc(54904);
        fread((void *)buffer_v1, 1, 54904, file);
        fclose(file);
    }

    void TearDown() { free((void *)buffer_v1); }

    ~ace_v1Fixture() {}
};

TEST_F(ace_v1Fixture, ace_v1_lz77)
{
    size_t destLen = 152089;
    size_t srcLen  = 54904;
    auto  *outBuf  = (uint8_t *)malloc(152089);

    auto err = ace_decompress_lz77(buffer_v1, srcLen, outBuf, &destLen, 20);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, 152089);

    auto crc = crc32_data(outBuf, 152089);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}

class ace_v2Fixture : public ::testing::Test
{
public:
    ace_v2Fixture() {}

protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];

        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/ace_v2_blocked.bin", path);

        FILE *file = fopen(filename, "rb");
        buffer_v2  = (const uint8_t *)malloc(50984);
        fread((void *)buffer_v2, 1, 50984, file);
        fclose(file);
    }

    void TearDown() { free((void *)buffer_v2); }

    ~ace_v2Fixture() {}
};

TEST_F(ace_v2Fixture, ace_v2_blocked)
{
    size_t destLen = 152089;
    size_t srcLen  = 50984;
    auto  *outBuf  = (uint8_t *)malloc(152089);

    auto err = ace_decompress_blocked(buffer_v2, srcLen, outBuf, &destLen, 20);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, 152089);

    auto crc = crc32_data(outBuf, 152089);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}
