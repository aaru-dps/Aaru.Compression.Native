/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2021 Natalia Portillo.
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
#include <cstdint>
#include <cstring>

#include "../apple_rle.h"
#include "../library.h"
#include "crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0x3525ef06

static const uint8_t* buffer;

class apple_rleFixture : public ::testing::Test
{
  public:
    apple_rleFixture()
    {
        // initialization;
        // can also be done in SetUp()
    }

  protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];

        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/apple_rle.bin", path);

        FILE* file = fopen(filename, "rb");
        buffer     = (const uint8_t*)malloc(1102);
        fread((void*)buffer, 1, 1102, file);
        fclose(file);
    }

    void TearDown() { free((void*)buffer); }

    ~apple_rleFixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(apple_rleFixture, apple_rle)
{
    auto* outBuf = (uint8_t*)malloc(32768);

    auto decoded = AARU_apple_rle_decode_buffer(outBuf, 32768, buffer, 1102);

    EXPECT_EQ(decoded, 20960);

    auto crc = crc32_data(outBuf, 20960);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}
