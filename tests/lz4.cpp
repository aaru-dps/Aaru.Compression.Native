/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2025 Natalia Portillo.
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

#include "../library.h"
#include "crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0x954bf76e  // CRC32 of whole 8MB data.bin file

static const uint8_t *buffer;
static int32_t buffer_size;

class lz4Fixture : public ::testing::Test
{
public:
    lz4Fixture()
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
        snprintf(filename, PATH_MAX, "%s/data/lz4.bin", path);

        FILE *file = fopen(filename, "rb");
        fseek(file, 0, SEEK_END);
        buffer_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        buffer = (const uint8_t *)malloc(buffer_size);
        fread((void *)buffer, 1, buffer_size, file);
        fclose(file);
    }

    void TearDown() { free((void *)buffer); }

    ~lz4Fixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(lz4Fixture, lz4)
{
    int32_t original_len = 8388608;  // 8MB
    auto *outBuf = (uint8_t *)malloc(original_len);

    auto decoded = AARU_lz4_decode_buffer(outBuf, original_len, buffer, buffer_size);

    EXPECT_GT(decoded, 0);
    EXPECT_EQ(decoded, original_len);

    auto crc = crc32_data(outBuf, original_len);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}

TEST_F(lz4Fixture, lz4Compress)
{
    int32_t        original_len = 8388608;
    int32_t        cmp_len      = original_len;
    int32_t        decmp_len    = original_len;
    char           path[PATH_MAX];
    char           filename[PATH_MAX * 2];
    FILE          *file;
    uint32_t       original_crc, decmp_crc;
    const uint8_t *original;
    uint8_t       *cmp_buffer;
    uint8_t       *decmp_buffer;
    int32_t        newSize;

    // Allocate buffers
    original     = (const uint8_t *)malloc(original_len);
    cmp_buffer   = (uint8_t *)malloc(cmp_len);
    decmp_buffer = (uint8_t *)malloc(decmp_len);

    // Read the file
    getcwd(path, PATH_MAX);
    snprintf(filename, PATH_MAX, "%s/data/data.bin", path);

    file = fopen(filename, "rb");
    fread((void *)original, 1, original_len, file);
    fclose(file);

    // Calculate the CRC
    original_crc = crc32_data(original, original_len);

    // Compress
    newSize = AARU_lz4_encode_buffer(cmp_buffer, cmp_len, original, original_len);
    EXPECT_GT(newSize, 0);
    cmp_len = newSize;

    // Decompress
    newSize   = AARU_lz4_decode_buffer(decmp_buffer, decmp_len, cmp_buffer, cmp_len);
    EXPECT_GT(newSize, 0);
    decmp_len = newSize;

    EXPECT_EQ(decmp_len, original_len);

    decmp_crc = crc32_data(decmp_buffer, decmp_len);

    // Free buffers
    free((void *)original);
    free(cmp_buffer);
    free(decmp_buffer);

    EXPECT_EQ(decmp_crc, original_crc);
}
