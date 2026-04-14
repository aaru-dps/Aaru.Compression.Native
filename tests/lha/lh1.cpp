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
#include "../../lha/lh1.h"
#include "../crc32.h"
#include "gtest/gtest.h"
#include "lha_helpers.h"

/* alice29.txt decompressed CRC32 */
#define EXPECTED_CRC32 0x66007dba
#define EXPECTED_SIZE  152089

static uint8_t       *lh1_payload;
static lha_test_info  lh1_info;

class lh1Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];

        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/lharc_lh1.lzh", path);

        lh1_payload = load_lha_payload(filename, &lh1_info);
    }

    void TearDown()
    {
        if(lh1_payload) free(lh1_payload);
    }
};

TEST_F(lh1Fixture, lh1_decompress)
{
    ASSERT_NE(lh1_payload, nullptr);
    EXPECT_EQ(lh1_info.uncompressed_size, (uint32_t)EXPECTED_SIZE);

    size_t   out_len = EXPECTED_SIZE;
    uint8_t *out_buf = (uint8_t *)malloc(out_len);

    int err = lha_decompress_lh1(lh1_payload, lh1_info.payload_size, out_buf, &out_len);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(out_len, (size_t)EXPECTED_SIZE);

    uint32_t crc = crc32_data(out_buf, (uint32_t)out_len);

    free(out_buf);

    EXPECT_EQ(crc, (uint32_t)EXPECTED_CRC32);
}
