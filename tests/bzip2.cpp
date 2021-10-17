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

#include "../3rdparty/bzip2/bzlib.h"
#include "crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0xc64059c0

static const uint8_t* buffer;

class bzip2Fixture : public ::testing::Test
{
  public:
    bzip2Fixture()
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
        snprintf(filename, PATH_MAX, "%s/data/bzip2.bz2", path);

        FILE* file = fopen(filename, "rb");
        buffer     = (const uint8_t*)malloc(1053934);
        fread((void*)buffer, 1, 1053934, file);
        fclose(file);
    }

    void TearDown() { free((void*)buffer); }

    ~bzip2Fixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(bzip2Fixture, bzip2)
{
    uint  real_size = 1048576;
    auto* outBuf    = (uint8_t*)malloc(1048576);

    auto bz_err = BZ2_bzBuffToBuffDecompress((char*)outBuf, &real_size, (char*)buffer, 1053934, 0, 0);

    EXPECT_EQ(bz_err, BZ_OK);

    EXPECT_EQ(real_size, 1048576);

    auto crc = crc32_data(outBuf, 1048576);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}
