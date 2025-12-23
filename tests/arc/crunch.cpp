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

static const uint8_t *buffer;

class crunchFixture : public ::testing::Test
{
public:
    crunchFixture()
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
        snprintf(filename, PATH_MAX, "%s/data/arccrunchnr.bin", path);

        FILE *file = fopen(filename, "rb");
        buffer     = (const uint8_t *)malloc(72537);
        fread((void *)buffer, 1, 72537, file);
        fclose(file);
    }

    void TearDown() { free((void *)buffer); }

    ~crunchFixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(crunchFixture, crunch)
{
    size_t destLen = 152089;
    size_t srcLen  = 72537;
    auto  *outBuf  = (uint8_t *)malloc(152089);

    auto err = arc_decompress_crunch_nrpack(buffer, srcLen, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, 152089);

    auto crc = crc32_data(outBuf, 152089);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}

class crunchDynamicFixture : public ::testing::Test
{
public:
    crunchDynamicFixture()
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
        snprintf(filename, PATH_MAX, "%s/data/arccrunch_dynamic.bin", path);

        FILE *file = fopen(filename, "rb");
        buffer     = (const uint8_t *)malloc(73189);
        fread((void *)buffer, 1, 73189, file);
        fclose(file);
    }

    void TearDown() { free((void *)buffer); }

    ~crunchDynamicFixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(crunchDynamicFixture, crunchDynamic)
{
    size_t destLen = 152089;
    size_t srcLen  = 73189;
    auto  *outBuf  = (uint8_t *)malloc(152089);

    auto err = arc_decompress_crunch_dynamic(buffer, srcLen, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, 152089);

    auto crc = crc32_data(outBuf, 152089);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}