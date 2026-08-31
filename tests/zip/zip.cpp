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

#include "../../library.h"
#include "../crc32.h"
#include "gtest/gtest.h"

/* alice29.txt: 152089 bytes, CRC32 = 0x66007dba */
#define EXPECTED_CRC32      0x66007dba
#define EXPECTED_OUTPUT_SIZE 152089

/* ZIP Shrink test: PKZIP1 ES.ZIP (method 1) */
#define SHRINK_COMPRESSED_SIZE 65014

static const uint8_t *shrink_buffer;

class ZipShrinkFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/zip_shrink.bin", path);

        FILE *file    = fopen(filename, "rb");
        shrink_buffer = (const uint8_t *)malloc(SHRINK_COMPRESSED_SIZE);
        fread((void *)shrink_buffer, 1, SHRINK_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)shrink_buffer); }
};

TEST_F(ZipShrinkFixture, ZipShrink)
{
    size_t  destLen = EXPECTED_OUTPUT_SIZE;
    auto   *outBuf  = (uint8_t *)malloc(EXPECTED_OUTPUT_SIZE);

    auto err = AARU_zip_shrink_decode_buffer(shrink_buffer, SHRINK_COMPRESSED_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, EXPECTED_OUTPUT_SIZE);

    auto crc = crc32_data(outBuf, EXPECTED_OUTPUT_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ZIP Implode test: PKZIP1 EI.ZIP (method 6, flags=0x0006: 8K dict + literals) */
#define IMPLODE_COMPRESSED_SIZE 60488

static const uint8_t *implode_buffer;

class ZipImplodeFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/zip_implode.bin", path);

        FILE *file     = fopen(filename, "rb");
        implode_buffer = (const uint8_t *)malloc(IMPLODE_COMPRESSED_SIZE);
        fread((void *)implode_buffer, 1, IMPLODE_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)implode_buffer); }
};

TEST_F(ZipImplodeFixture, ZipImplode)
{
    size_t  destLen = EXPECTED_OUTPUT_SIZE;
    auto   *outBuf  = (uint8_t *)malloc(EXPECTED_OUTPUT_SIZE);

    /* large_dictionary=1 (8K), has_literals=1 (3 trees) from flags 0x0006 */
    auto err = AARU_zip_implode_decode_buffer(implode_buffer, IMPLODE_COMPRESSED_SIZE, outBuf, &destLen, 1, 1);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, EXPECTED_OUTPUT_SIZE);

    auto crc = crc32_data(outBuf, EXPECTED_OUTPUT_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ZIP Deflate64 test: 7-Zip DEFLATE64 1FILE.ZIP (method 9) */
#define DEFLATE64_COMPRESSED_SIZE 50564

static const uint8_t *deflate64_buffer;

class ZipDeflate64Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/zip_deflate64.bin", path);

        FILE *file       = fopen(filename, "rb");
        deflate64_buffer = (const uint8_t *)malloc(DEFLATE64_COMPRESSED_SIZE);
        fread((void *)deflate64_buffer, 1, DEFLATE64_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)deflate64_buffer); }
};

TEST_F(ZipDeflate64Fixture, ZipDeflate64)
{
    size_t  destLen = EXPECTED_OUTPUT_SIZE;
    auto   *outBuf  = (uint8_t *)malloc(EXPECTED_OUTPUT_SIZE);

    auto err = AARU_zip_deflate64_decode_buffer(deflate64_buffer, DEFLATE64_COMPRESSED_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, EXPECTED_OUTPUT_SIZE);

    auto crc = crc32_data(outBuf, EXPECTED_OUTPUT_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ZIP PPMd test: 7-Zip PPMd 1FILE.ZIP (method 98, variant I) */
/* Parameters from info word 0x0037: maxorder=8, suballocsize=4MB, restoration=0 */
#define PPMD_COMPRESSED_SIZE 38627

static const uint8_t *ppmd_buffer;

class ZipPPMdFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/zip_ppmd.bin", path);

        FILE *file  = fopen(filename, "rb");
        ppmd_buffer = (const uint8_t *)malloc(PPMD_COMPRESSED_SIZE);
        fread((void *)ppmd_buffer, 1, PPMD_COMPRESSED_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)ppmd_buffer); }
};

TEST_F(ZipPPMdFixture, ZipPPMd)
{
    auto *outBuf = (uint8_t *)malloc(EXPECTED_OUTPUT_SIZE);

    /* maxorder=8, suballocsize=4194304 (4MB), restoration=0 (restart) */
    size_t destLen = EXPECTED_OUTPUT_SIZE;

    auto err = AARU_zip_ppmd_decode_buffer(ppmd_buffer, PPMD_COMPRESSED_SIZE, outBuf, &destLen, 8, 4194304, 0);

    EXPECT_EQ(err, 0);

    auto crc = crc32_data(outBuf, EXPECTED_OUTPUT_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}
