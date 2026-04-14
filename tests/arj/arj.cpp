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

#define EXPECTED_CRC32   0x66007DBA
#define EXPECTED_ORIGSIZE 152089

/* ---- Method 1 ---- */

static const uint8_t *buffer_m1;

class arj_m1Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/arj_m1.bin", path);
        FILE *file = fopen(filename, "rb");
        buffer_m1  = (const uint8_t *)malloc(55125);
        fread((void *)buffer_m1, 1, 55125, file);
        fclose(file);
    }
    void TearDown() { free((void *)buffer_m1); }
};

TEST_F(arj_m1Fixture, arj_method1)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = arj_decompress_method1(buffer_m1, 55125, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- Method 2 ---- */

static const uint8_t *buffer_m2;

class arj_m2Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/arj_m2.bin", path);
        FILE *file = fopen(filename, "rb");
        buffer_m2  = (const uint8_t *)malloc(55721);
        fread((void *)buffer_m2, 1, 55721, file);
        fclose(file);
    }
    void TearDown() { free((void *)buffer_m2); }
};

TEST_F(arj_m2Fixture, arj_method2)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = arj_decompress_method2(buffer_m2, 55721, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- Method 3 ---- */

static const uint8_t *buffer_m3;

class arj_m3Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/arj_m3.bin", path);
        FILE *file = fopen(filename, "rb");
        buffer_m3  = (const uint8_t *)malloc(59190);
        fread((void *)buffer_m3, 1, 59190, file);
        fclose(file);
    }
    void TearDown() { free((void *)buffer_m3); }
};

TEST_F(arj_m3Fixture, arj_method3)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = arj_decompress_method3(buffer_m3, 59190, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- Method 4 (Fastest) ---- */

static const uint8_t *buffer_m4;

class arj_m4Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/arj_m4.bin", path);
        FILE *file = fopen(filename, "rb");
        buffer_m4  = (const uint8_t *)malloc(66519);
        fread((void *)buffer_m4, 1, 66519, file);
        fclose(file);
    }
    void TearDown() { free((void *)buffer_m4); }
};

TEST_F(arj_m4Fixture, arj_method4_fastest)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = arj_decompress_fastest(buffer_m4, 66519, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}
