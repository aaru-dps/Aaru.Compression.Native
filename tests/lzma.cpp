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

#include "../3rdparty/lzma-21.03beta/C/LzmaLib.h"
#include "../library.h"
#include "crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0x954bf76e

static const uint8_t* buffer;

class lzmaFixture : public ::testing::Test
{
  public:
    lzmaFixture()
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
        snprintf(filename, PATH_MAX, "%s/data/lzma.bin", path);

        FILE* file = fopen(filename, "rb");
        buffer     = (const uint8_t*)malloc(1200275);
        fread((void*)buffer, 1, 1200275, file);
        fclose(file);
    }

    void TearDown() { free((void*)buffer); }

    ~lzmaFixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(lzmaFixture, lzma)
{
    uint8_t params[] = {0x5D, 0x00, 0x00, 0x00, 0x02};
    size_t  destLen  = 8388608;
    size_t  srcLen   = 1200275;
    auto*   outBuf   = (uint8_t*)malloc(8388608);

    auto err = LzmaUncompress(outBuf, &destLen, buffer, &srcLen, params, 5);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, 8388608);

    auto crc = crc32_data(outBuf, 8388608);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}
