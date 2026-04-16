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
#include <cstring>

#include "../../library.h"
#include "../crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32    0x66007dba
#define EXPECTED_ORIGSIZE 152089

/* ── RAR 1.5 (method 0x35, Best) ── */

class rar15Fixture : public ::testing::Test
{
  protected:
    uint8_t *buffer;
    size_t   srcLen;

    void SetUp() override
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/data/rar15_m5.bin", cwd);

        FILE *f = fopen(path, "rb");
        ASSERT_NE(f, nullptr) << "Cannot open " << path;
        fseek(f, 0, SEEK_END);
        srcLen = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (uint8_t *)malloc(srcLen);
        ASSERT_NE(buffer, nullptr);
        fread(buffer, 1, srcLen, f);
        fclose(f);
    }

    void TearDown() override { free(buffer); }
};

TEST_F(rar15Fixture, rar15_decompress)
{
    size_t   destLen = EXPECTED_ORIGSIZE;
    uint8_t *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);
    ASSERT_NE(outBuf, nullptr);

    int err = rar15_decompress(buffer, srcLen, outBuf, &destLen);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    uint32_t crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, (uint32_t)EXPECTED_CRC32);
}

/* ── RAR 2.0 (method 0x33, Normal / Defaults) ── */

class rar20Fixture : public ::testing::Test
{
  protected:
    uint8_t *buffer;
    size_t   srcLen;

    void SetUp() override
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/data/rar20_default.bin", cwd);

        FILE *f = fopen(path, "rb");
        ASSERT_NE(f, nullptr) << "Cannot open " << path;
        fseek(f, 0, SEEK_END);
        srcLen = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (uint8_t *)malloc(srcLen);
        ASSERT_NE(buffer, nullptr);
        fread(buffer, 1, srcLen, f);
        fclose(f);
    }

    void TearDown() override { free(buffer); }
};

TEST_F(rar20Fixture, rar20_decompress)
{
    size_t   destLen = EXPECTED_ORIGSIZE;
    uint8_t *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);
    ASSERT_NE(outBuf, nullptr);

    int err = rar20_decompress(buffer, srcLen, outBuf, &destLen);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    uint32_t crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, (uint32_t)EXPECTED_CRC32);
}

/* ── RAR 3.0 (method 0x33, Normal / Defaults) ── */

class rar30Fixture : public ::testing::Test
{
  protected:
    uint8_t *buffer;
    size_t   srcLen;

    void SetUp() override
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/data/rar30_default.bin", cwd);

        FILE *f = fopen(path, "rb");
        ASSERT_NE(f, nullptr) << "Cannot open " << path;
        fseek(f, 0, SEEK_END);
        srcLen = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (uint8_t *)malloc(srcLen);
        ASSERT_NE(buffer, nullptr);
        fread(buffer, 1, srcLen, f);
        fclose(f);
    }

    void TearDown() override { free(buffer); }
};

TEST_F(rar30Fixture, rar30_decompress)
{
    size_t   destLen = EXPECTED_ORIGSIZE;
    uint8_t *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);
    ASSERT_NE(outBuf, nullptr);

    int err = rar30_decompress(buffer, srcLen, outBuf, &destLen);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    uint32_t crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, (uint32_t)EXPECTED_CRC32);
}

/* ── RAR 5.0 (method 3, Normal / Defaults) ── */

class rar50Fixture : public ::testing::Test
{
  protected:
    uint8_t *buffer;
    size_t   srcLen;

    void SetUp() override
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/data/rar50_default.bin", cwd);

        FILE *f = fopen(path, "rb");
        ASSERT_NE(f, nullptr) << "Cannot open " << path;
        fseek(f, 0, SEEK_END);
        srcLen = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (uint8_t *)malloc(srcLen);
        ASSERT_NE(buffer, nullptr);
        fread(buffer, 1, srcLen, f);
        fclose(f);
    }

    void TearDown() override { free(buffer); }
};

TEST_F(rar50Fixture, rar50_decompress)
{
    size_t   destLen    = EXPECTED_ORIGSIZE;
    size_t   windowSize = 262144; /* 256KB dictionary from archive header */
    uint8_t *outBuf     = (uint8_t *)malloc(EXPECTED_ORIGSIZE);
    ASSERT_NE(outBuf, nullptr);

    int err = rar50_decompress(buffer, srcLen, outBuf, &destLen, windowSize);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    uint32_t crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, (uint32_t)EXPECTED_CRC32);
}

/* ── RAR 5.0 method 3/4/5 (Calgary obj2, with filters) ── */

#define RAR50_OBJ2_CRC32    0x3ae33007
#define RAR50_OBJ2_ORIGSIZE 246814
#define RAR50_OBJ2_WINSIZE  1048576 /* 1MB dictionary */

class rar50M3Fixture : public ::testing::Test
{
  protected:
    uint8_t *buffer;
    size_t   srcLen;

    void SetUp() override
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/data/rar50_m3.bin", cwd);

        FILE *f = fopen(path, "rb");
        ASSERT_NE(f, nullptr) << "Cannot open " << path;
        fseek(f, 0, SEEK_END);
        srcLen = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (uint8_t *)malloc(srcLen);
        ASSERT_NE(buffer, nullptr);
        fread(buffer, 1, srcLen, f);
        fclose(f);
    }

    void TearDown() override { free(buffer); }
};

TEST_F(rar50M3Fixture, rar50_m3_decompress)
{
    size_t   destLen = RAR50_OBJ2_ORIGSIZE;
    uint8_t *outBuf  = (uint8_t *)malloc(RAR50_OBJ2_ORIGSIZE);
    ASSERT_NE(outBuf, nullptr);

    int err = rar50_decompress(buffer, srcLen, outBuf, &destLen, RAR50_OBJ2_WINSIZE);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)RAR50_OBJ2_ORIGSIZE);

    uint32_t crc = crc32_data(outBuf, RAR50_OBJ2_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, (uint32_t)RAR50_OBJ2_CRC32);
}

class rar50M4Fixture : public ::testing::Test
{
  protected:
    uint8_t *buffer;
    size_t   srcLen;

    void SetUp() override
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/data/rar50_m4.bin", cwd);

        FILE *f = fopen(path, "rb");
        ASSERT_NE(f, nullptr) << "Cannot open " << path;
        fseek(f, 0, SEEK_END);
        srcLen = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (uint8_t *)malloc(srcLen);
        ASSERT_NE(buffer, nullptr);
        fread(buffer, 1, srcLen, f);
        fclose(f);
    }

    void TearDown() override { free(buffer); }
};

TEST_F(rar50M4Fixture, rar50_m4_decompress)
{
    size_t   destLen = RAR50_OBJ2_ORIGSIZE;
    uint8_t *outBuf  = (uint8_t *)malloc(RAR50_OBJ2_ORIGSIZE);
    ASSERT_NE(outBuf, nullptr);

    int err = rar50_decompress(buffer, srcLen, outBuf, &destLen, RAR50_OBJ2_WINSIZE);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)RAR50_OBJ2_ORIGSIZE);

    uint32_t crc = crc32_data(outBuf, RAR50_OBJ2_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, (uint32_t)RAR50_OBJ2_CRC32);
}

class rar50M5Fixture : public ::testing::Test
{
  protected:
    uint8_t *buffer;
    size_t   srcLen;

    void SetUp() override
    {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/data/rar50_m5.bin", cwd);

        FILE *f = fopen(path, "rb");
        ASSERT_NE(f, nullptr) << "Cannot open " << path;
        fseek(f, 0, SEEK_END);
        srcLen = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (uint8_t *)malloc(srcLen);
        ASSERT_NE(buffer, nullptr);
        fread(buffer, 1, srcLen, f);
        fclose(f);
    }

    void TearDown() override { free(buffer); }
};

TEST_F(rar50M5Fixture, rar50_m5_decompress)
{
    size_t   destLen = RAR50_OBJ2_ORIGSIZE;
    uint8_t *outBuf  = (uint8_t *)malloc(RAR50_OBJ2_ORIGSIZE);
    ASSERT_NE(outBuf, nullptr);

    int err = rar50_decompress(buffer, srcLen, outBuf, &destLen, RAR50_OBJ2_WINSIZE);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)RAR50_OBJ2_ORIGSIZE);

    uint32_t crc = crc32_data(outBuf, RAR50_OBJ2_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, (uint32_t)RAR50_OBJ2_CRC32);
}
