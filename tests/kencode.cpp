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

#include "../library.h"
#include "../kencode.h"
#include "crc32.h"
#include "gtest/gtest.h"

// DOS_720.img chunk 0: boot sector with mixed data/matches (2495 → 10240)
#define KC_BOOT_COMP_SIZE   2495
#define KC_BOOT_DECOMP_SIZE 10240
#define KC_BOOT_CRC32       0x18DDE60Cu

// DOS_720.img chunk 47: mostly zero data, high compression (442 → 10240)
#define KC_ZEROS_COMP_SIZE   442
#define KC_ZEROS_DECOMP_SIZE 10240
#define KC_ZEROS_CRC32       0x271DDE9Au

// HFS_800.img chunk 2: HFS filesystem data (4579 → 10240)
#define KC_HFS_COMP_SIZE   4579
#define KC_HFS_DECOMP_SIZE 10240
#define KC_HFS_CRC32       0x72B8E8F1u

// HFS_DMF.img chunk 77: last chunk, non-standard 6144 byte output (3076 → 6144)
#define KC_LAST_COMP_SIZE   3076
#define KC_LAST_DECOMP_SIZE 6144
#define KC_LAST_CRC32       0xA0B2EAEFu

// DOS_SP_5Mb.img chunk 0: 5MB disk image first chunk (2017 → 10240)
#define KC_LARGE_COMP_SIZE   2017
#define KC_LARGE_DECOMP_SIZE 10240
#define KC_LARGE_CRC32       0xA584FF1Au

static uint8_t *read_test_file(const char *name, size_t expected_size)
{
    char path[PATH_MAX];
    char filename[PATH_MAX];

    getcwd(path, PATH_MAX);
    snprintf(filename, PATH_MAX, "%s/data/%s", path, name);

    FILE *file = fopen(filename, "rb");
    if(file == nullptr) return nullptr;

    auto *buf = (uint8_t *)malloc(expected_size);
    fread(buf, 1, expected_size, file);
    fclose(file);
    return buf;
}

/* ---- Boot sector: mixed data and matches ---- */

static const uint8_t *kc_boot_buffer;

class KencodeBootFixture : public ::testing::Test
{
protected:
    void SetUp() { kc_boot_buffer = read_test_file("kencode_boot.bin", KC_BOOT_COMP_SIZE); }

    void TearDown() { free((void *)kc_boot_buffer); }
};

TEST_F(KencodeBootFixture, kencode_boot)
{
    auto  *outBuf  = (uint8_t *)malloc(KC_BOOT_DECOMP_SIZE);
    size_t destLen = KC_BOOT_DECOMP_SIZE;

    auto err = AARU_kencode_decode_buffer(kc_boot_buffer, KC_BOOT_COMP_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)KC_BOOT_DECOMP_SIZE);

    auto crc = crc32_data(outBuf, KC_BOOT_DECOMP_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, KC_BOOT_CRC32);
}

/* ---- Mostly zeros: high compression ratio (23:1) ---- */

static const uint8_t *kc_zeros_buffer;

class KencodeZerosFixture : public ::testing::Test
{
protected:
    void SetUp() { kc_zeros_buffer = read_test_file("kencode_zeros.bin", KC_ZEROS_COMP_SIZE); }

    void TearDown() { free((void *)kc_zeros_buffer); }
};

TEST_F(KencodeZerosFixture, kencode_zeros)
{
    auto  *outBuf  = (uint8_t *)malloc(KC_ZEROS_DECOMP_SIZE);
    size_t destLen = KC_ZEROS_DECOMP_SIZE;

    auto err = AARU_kencode_decode_buffer(kc_zeros_buffer, KC_ZEROS_COMP_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)KC_ZEROS_DECOMP_SIZE);

    auto crc = crc32_data(outBuf, KC_ZEROS_DECOMP_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, KC_ZEROS_CRC32);
}

/* ---- HFS filesystem data ---- */

static const uint8_t *kc_hfs_buffer;

class KencodeHfsFixture : public ::testing::Test
{
protected:
    void SetUp() { kc_hfs_buffer = read_test_file("kencode_hfs.bin", KC_HFS_COMP_SIZE); }

    void TearDown() { free((void *)kc_hfs_buffer); }
};

TEST_F(KencodeHfsFixture, kencode_hfs)
{
    auto  *outBuf  = (uint8_t *)malloc(KC_HFS_DECOMP_SIZE);
    size_t destLen = KC_HFS_DECOMP_SIZE;

    auto err = AARU_kencode_decode_buffer(kc_hfs_buffer, KC_HFS_COMP_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)KC_HFS_DECOMP_SIZE);

    auto crc = crc32_data(outBuf, KC_HFS_DECOMP_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, KC_HFS_CRC32);
}

/* ---- Last chunk: non-standard 6144-byte output ---- */

static const uint8_t *kc_last_buffer;

class KencodeLastFixture : public ::testing::Test
{
protected:
    void SetUp() { kc_last_buffer = read_test_file("kencode_last.bin", KC_LAST_COMP_SIZE); }

    void TearDown() { free((void *)kc_last_buffer); }
};

TEST_F(KencodeLastFixture, kencode_last)
{
    auto  *outBuf  = (uint8_t *)malloc(KC_LAST_DECOMP_SIZE);
    size_t destLen = KC_LAST_DECOMP_SIZE;

    auto err = AARU_kencode_decode_buffer(kc_last_buffer, KC_LAST_COMP_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)KC_LAST_DECOMP_SIZE);

    auto crc = crc32_data(outBuf, KC_LAST_DECOMP_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, KC_LAST_CRC32);
}

/* ---- 5MB disk image chunk ---- */

static const uint8_t *kc_large_buffer;

class KencodeLargeFixture : public ::testing::Test
{
protected:
    void SetUp() { kc_large_buffer = read_test_file("kencode_large.bin", KC_LARGE_COMP_SIZE); }

    void TearDown() { free((void *)kc_large_buffer); }
};

TEST_F(KencodeLargeFixture, kencode_large)
{
    auto  *outBuf  = (uint8_t *)malloc(KC_LARGE_DECOMP_SIZE);
    size_t destLen = KC_LARGE_DECOMP_SIZE;

    auto err = AARU_kencode_decode_buffer(kc_large_buffer, KC_LARGE_COMP_SIZE, outBuf, &destLen);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(destLen, (size_t)KC_LARGE_DECOMP_SIZE);

    auto crc = crc32_data(outBuf, KC_LARGE_DECOMP_SIZE);
    free(outBuf);

    EXPECT_EQ(crc, KC_LARGE_CRC32);
}
