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

#include "../3rdparty/lzfse/src/lzfse.h"
#include "crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0xc64059c0

static const uint8_t* buffer;

class lzfseFixture : public ::testing::Test
{
  public:
    lzfseFixture()
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
        snprintf(filename, PATH_MAX, "%s/data/lzfse.bin", path);

        FILE* file = fopen(filename, "rb");
        buffer     = (const uint8_t*)malloc(1059299);
        fread((void*)buffer, 1, 1059299, file);
        fclose(file);
    }

    void TearDown() { free((void*)buffer); }

    ~lzfseFixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(lzfseFixture, lzfse)
{
    auto* outBuf = (uint8_t*)malloc(1048576);

    auto decoded = lzfse_decode_buffer(outBuf, 1048576, buffer, 1059299, NULL);

    EXPECT_EQ(decoded, 1048576);

    auto crc = crc32_data(outBuf, 1048576);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}
