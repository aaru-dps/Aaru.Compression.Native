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

/* All test files decompress to alice29.txt */
#define EXPECTED_CRC32    0x66007DBA
#define EXPECTED_ORIGSIZE 152089

/* ---- StuffIt method 2: UNIX Compress ---- */

#define COMPRESS_SIZE 67146

static const uint8_t *compress_buffer;

class StuffitCompressFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX], filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/stuffit_compress.bin", path);
        FILE *file      = fopen(filename, "rb");
        compress_buffer = (const uint8_t *)malloc(COMPRESS_SIZE);
        fread((void *)compress_buffer, 1, COMPRESS_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)compress_buffer); }
};

TEST_F(StuffitCompressFixture, stuffit_compress)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_stuffit_compress_decode_buffer(outBuf, &destLen, compress_buffer, COMPRESS_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- StuffIt method 13: LZ+Huffman (v3.0) ---- */

#define METHOD13_SIZE 54981

static const uint8_t *method13_buffer;

class StuffitMethod13Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX], filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/stuffit_method13.bin", path);
        FILE *file      = fopen(filename, "rb");
        method13_buffer = (const uint8_t *)malloc(METHOD13_SIZE);
        fread((void *)method13_buffer, 1, METHOD13_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)method13_buffer); }
};

TEST_F(StuffitMethod13Fixture, stuffit_method13)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_stuffit_method13_decode_buffer(outBuf, &destLen, method13_buffer, METHOD13_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- StuffIt method 13: LZ+Huffman (v5.0) ---- */

#define METHOD13_V5_SIZE 55033

static const uint8_t *method13_v5_buffer;

class StuffitMethod13V5Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX], filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/stuffit_method13_v5.bin", path);
        FILE *file         = fopen(filename, "rb");
        method13_v5_buffer = (const uint8_t *)malloc(METHOD13_V5_SIZE);
        fread((void *)method13_v5_buffer, 1, METHOD13_V5_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)method13_v5_buffer); }
};

TEST_F(StuffitMethod13V5Fixture, stuffit_method13_v5)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_stuffit_method13_decode_buffer(outBuf, &destLen, method13_v5_buffer, METHOD13_V5_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- StuffIt method 15: Arsenic ---- */

#define ARSENIC_SIZE 42756

static const uint8_t *arsenic_buffer;

class StuffitArsenicFixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX], filename[PATH_MAX];
        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/stuffit_arsenic.bin", path);
        FILE *file     = fopen(filename, "rb");
        arsenic_buffer = (const uint8_t *)malloc(ARSENIC_SIZE);
        fread((void *)arsenic_buffer, 1, ARSENIC_SIZE, file);
        fclose(file);
    }

    void TearDown() { free((void *)arsenic_buffer); }
};

TEST_F(StuffitArsenicFixture, stuffit_arsenic)
{
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(EXPECTED_ORIGSIZE);

    auto err = AARU_stuffit_arsenic_decode_buffer(outBuf, &destLen, arsenic_buffer, ARSENIC_SIZE);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, EXPECTED_ORIGSIZE);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ============ StuffIt X Tests ============ */

/* Helper to load a test file */
static uint8_t *load_test_file(const char *name, size_t expected_size)
{
    char path[PATH_MAX], filename[PATH_MAX];
    getcwd(path, PATH_MAX);
    snprintf(filename, PATH_MAX, "%s/data/%s", path, name);
    FILE *file = fopen(filename, "rb");
    if(!file) return NULL;
    auto *buf = (uint8_t *)malloc(expected_size);
    fread(buf, 1, expected_size, file);
    fclose(file);
    return buf;
}

/* ---- StuffIt X: Brimstone (PPMd VariantG) + English ---- */

#define BRIMSTONE_SIZE 35896

class StuffitxBrimstoneFixture : public ::testing::Test
{
protected:
    const uint8_t *buf;

    void SetUp() { buf = load_test_file("stuffitx_brimstone.bin", BRIMSTONE_SIZE); }

    void TearDown() { free((void *)buf); }
};

TEST_F(StuffitxBrimstoneFixture, stuffitx_brimstone)
{
    /* First 2 bytes: exponent (0x18=24 -> allocsize=16MB), order (0x08) */
    int exponent  = buf[0];
    int order     = buf[1];
    int allocsize = 1 << exponent;

    /* Decompress with Brimstone */
    size_t midLen = EXPECTED_ORIGSIZE * 2;
    auto  *midBuf = (uint8_t *)malloc(midLen);

    auto err = AARU_stuffitx_brimstone_decode_buffer(midBuf, &midLen, buf + 2, BRIMSTONE_SIZE - 2, order, allocsize);
    EXPECT_EQ(err, 0);
    EXPECT_GT(midLen, (size_t)0);

    /* Apply English decode */
    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(destLen);

    err = AARU_stuffitx_english_decode_buffer(outBuf, &destLen, midBuf, midLen);
    free(midBuf);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, destLen);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- StuffIt X: Cyanide (BWT + Ternary Range) + English ---- */

#define CYANIDE_SIZE 37743

class StuffitxCyanideFixture : public ::testing::Test
{
protected:
    const uint8_t *buf;

    void SetUp() { buf = load_test_file("stuffitx_cyanide.bin", CYANIDE_SIZE); }

    void TearDown() { free((void *)buf); }
};

TEST_F(StuffitxCyanideFixture, stuffitx_cyanide)
{
    /* No parameter bytes - data starts immediately */
    size_t midLen = EXPECTED_ORIGSIZE * 2;
    auto  *midBuf = (uint8_t *)malloc(midLen);

    auto err = AARU_stuffitx_cyanide_decode_buffer(midBuf, &midLen, buf, CYANIDE_SIZE);
    EXPECT_EQ(err, 0);
    EXPECT_GT(midLen, (size_t)0);

    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(destLen);

    err = AARU_stuffitx_english_decode_buffer(outBuf, &destLen, midBuf, midLen);
    free(midBuf);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, destLen);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- StuffIt X: Darkhorse (LZSS + Range Coder) + English ---- */

#define DARKHORSE_SIZE 42623

class StuffitxDarkhorseFixture : public ::testing::Test
{
protected:
    const uint8_t *buf;

    void SetUp() { buf = load_test_file("stuffitx_darkhorse.bin", DARKHORSE_SIZE); }

    void TearDown() { free((void *)buf); }
};

TEST_F(StuffitxDarkhorseFixture, stuffitx_darkhorse)
{
    /* First byte: window_bits (0x14=20) -> window = max(1<<20, 0x100000) = 1MB */
    int window_bits = buf[0];

    size_t midLen = EXPECTED_ORIGSIZE * 2;
    auto  *midBuf = (uint8_t *)malloc(midLen);

    auto err = AARU_stuffitx_darkhorse_decode_buffer(midBuf, &midLen, buf + 1, DARKHORSE_SIZE - 1, window_bits);
    EXPECT_EQ(err, 0);
    EXPECT_GT(midLen, (size_t)0);

    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(destLen);

    err = AARU_stuffitx_english_decode_buffer(outBuf, &destLen, midBuf, midLen);
    free(midBuf);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, destLen);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- StuffIt X: Deflate (Modified) + English ---- */

#define SITX_DEFLATE_SIZE 50567

class StuffitxDeflateFixture : public ::testing::Test
{
protected:
    const uint8_t *buf;

    void SetUp() { buf = load_test_file("stuffitx_deflate.bin", SITX_DEFLATE_SIZE); }

    void TearDown() { free((void *)buf); }
};

TEST_F(StuffitxDeflateFixture, stuffitx_deflate)
{
    /* First byte: window_size (0x0f=15, must be 15) */
    size_t midLen = EXPECTED_ORIGSIZE * 2;
    auto  *midBuf = (uint8_t *)malloc(midLen);

    auto err = AARU_stuffitx_deflate_decode_buffer(midBuf, &midLen, buf + 1, SITX_DEFLATE_SIZE - 1);
    EXPECT_EQ(err, 0);
    EXPECT_GT(midLen, (size_t)0);

    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(destLen);

    err = AARU_stuffitx_english_decode_buffer(outBuf, &destLen, midBuf, midLen);
    free(midBuf);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, destLen);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}

/* ---- StuffIt X: Blend (Multiplexer) + English ---- */

#define BLEND_SIZE 35962

class StuffitxBlendFixture : public ::testing::Test
{
protected:
    const uint8_t *buf;

    void SetUp() { buf = load_test_file("stuffitx_blend.bin", BLEND_SIZE); }

    void TearDown() { free((void *)buf); }
};

TEST_F(StuffitxBlendFixture, stuffitx_blend)
{
    /* No parameter bytes */
    size_t midLen = EXPECTED_ORIGSIZE * 2;
    auto  *midBuf = (uint8_t *)malloc(midLen);

    auto err = AARU_stuffitx_blend_decode_buffer(midBuf, &midLen, buf, BLEND_SIZE);
    EXPECT_EQ(err, 0);
    EXPECT_GT(midLen, (size_t)0);

    size_t destLen = EXPECTED_ORIGSIZE;
    auto  *outBuf  = (uint8_t *)malloc(destLen);

    err = AARU_stuffitx_english_decode_buffer(outBuf, &destLen, midBuf, midLen);
    free(midBuf);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)EXPECTED_ORIGSIZE);

    auto crc = crc32_data(outBuf, destLen);
    free(outBuf);
    EXPECT_EQ(crc, EXPECTED_CRC32);
}
