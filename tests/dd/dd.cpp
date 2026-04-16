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

#include <unistd.h>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "../../library.h"
#include "../crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32    0x66007DBA
#define EXPECTED_ORIGSIZE 152089

/* ---- ADn method 9 (AD1) ---- */

#define ADN_COMPRESSED_SIZE 102392

static const uint8_t *adn_buffer;

class DdAdnFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/dd_ad1.bin", path);
        FILE *file = fopen(filename, "rb");
        adn_buffer = (const uint8_t *)malloc(ADN_COMPRESSED_SIZE);
        fread((void *)adn_buffer, 1, ADN_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)adn_buffer); }
};

TEST_F(DdAdnFixture, dd_adn)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_dd_adn_decode_buffer(outBuf, &destLen, adn_buffer, ADN_COMPRESSED_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- ADn method 6 (AD2) ---- */

#define ADN2_COMPRESSED_SIZE 77708

static const uint8_t *adn2_buffer;

class DdAdn2Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/dd_ad2.bin", path);
        FILE *file  = fopen(filename, "rb");
        adn2_buffer = (const uint8_t *)malloc(ADN2_COMPRESSED_SIZE);
        fread((void *)adn2_buffer, 1, ADN2_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)adn2_buffer); }
};

TEST_F(DdAdn2Fixture, dd_adn2)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_dd_adn_decode_buffer(outBuf, &destLen, adn2_buffer, ADN2_COMPRESSED_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- DDn method 10 (DD1 - fastest) ---- */

#define DDN1_COMPRESSED_SIZE 59903

static const uint8_t *ddn1_buffer;

class DdDdn1Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/dd_dd1.bin", path);
        FILE *file  = fopen(filename, "rb");
        ddn1_buffer = (const uint8_t *)malloc(DDN1_COMPRESSED_SIZE);
        fread((void *)ddn1_buffer, 1, DDN1_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)ddn1_buffer); }
};

TEST_F(DdDdn1Fixture, dd_ddn1)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_dd_ddn_decode_buffer(outBuf, &destLen, ddn1_buffer, DDN1_COMPRESSED_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- DDn method 10 (DD2 - normal) ---- */

#define DDN2_COMPRESSED_SIZE 54219

static const uint8_t *ddn2_buffer;

class DdDdn2Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/dd_dd2.bin", path);
        FILE *file  = fopen(filename, "rb");
        ddn2_buffer = (const uint8_t *)malloc(DDN2_COMPRESSED_SIZE);
        fread((void *)ddn2_buffer, 1, DDN2_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)ddn2_buffer); }
};

TEST_F(DdDdn2Fixture, dd_ddn2)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_dd_ddn_decode_buffer(outBuf, &destLen, ddn2_buffer, DDN2_COMPRESSED_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- DDn method 10 (DD3 - best) ---- */

#define DDN3_COMPRESSED_SIZE 52881

static const uint8_t *ddn3_buffer;

class DdDdn3Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/dd_dd3.bin", path);
        FILE *file  = fopen(filename, "rb");
        ddn3_buffer = (const uint8_t *)malloc(DDN3_COMPRESSED_SIZE);
        fread((void *)ddn3_buffer, 1, DDN3_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)ddn3_buffer); }
};

TEST_F(DdDdn3Fixture, dd_ddn3)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_dd_ddn_decode_buffer(outBuf, &destLen, ddn3_buffer, DDN3_COMPRESSED_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}
