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

#include "../3rdparty/zstd-1.5.0/lib/zstd.h"
#include "../library.h"
#include "crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0xc64059c0

static const uint8_t* buffer;

class zstdFixture : public ::testing::Test
{
  public:
    zstdFixture()
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
        snprintf(filename, PATH_MAX, "%s/data/zstd.zst", path);

        FILE* file = fopen(filename, "rb");
        buffer     = (const uint8_t*)malloc(1048613);
        fread((void*)buffer, 1, 1048613, file);
        fclose(file);
    }

    void TearDown() { free((void*)buffer); }

    ~zstdFixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(zstdFixture, zstd)
{
    auto* outBuf = (uint8_t*)malloc(1048576);

    auto decoded = ZSTD_decompress(outBuf, 1048576, buffer, 1048613);

    EXPECT_EQ(decoded, 1048576);

    auto crc = crc32_data(outBuf, 1048576);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}

TEST_F(zstdFixture, zstdCompress)
{
    size_t         original_len = 8388608;
    uint           cmp_len      = original_len;
    uint           decmp_len    = original_len;
    char           path[PATH_MAX];
    char           filename[PATH_MAX * 2];
    FILE*          file;
    uint32_t       original_crc, decmp_crc;
    const uint8_t* original;
    uint8_t*       cmp_buffer;
    uint8_t*       decmp_buffer;
    size_t         newSize;

    // Allocate buffers
    original     = (const uint8_t*)malloc(original_len);
    cmp_buffer   = (uint8_t*)malloc(cmp_len);
    decmp_buffer = (uint8_t*)malloc(decmp_len);

    // Read the file
    getcwd(path, PATH_MAX);
    snprintf(filename, PATH_MAX, "%s/data/data.bin", path);

    file = fopen(filename, "rb");
    fread((void*)original, 1, original_len, file);
    fclose(file);

    // Calculate the CRC
    original_crc = crc32_data(original, original_len);

    // Compress
    newSize = ZSTD_compress(cmp_buffer, cmp_len, original, original_len, 22);
    cmp_len = newSize;

    // Decompress
    newSize   = ZSTD_decompress(decmp_buffer, decmp_len, cmp_buffer, cmp_len);
    decmp_len = newSize;

    EXPECT_EQ(decmp_len, original_len);

    decmp_crc = crc32_data(decmp_buffer, decmp_len);

    // Free buffers
    free((void*)original);
    free(cmp_buffer);
    free(decmp_buffer);

    EXPECT_EQ(decmp_crc, original_crc);
}
