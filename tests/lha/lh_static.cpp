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
#include "../../lha/lh_static.h"
#include "../crc32.h"
#include "gtest/gtest.h"
#include "lha_helpers.h"

/* alice29.txt decompressed CRC32 (same as zoo/lh5 test) */
#define EXPECTED_CRC32 0x66007dba
#define EXPECTED_SIZE  152089

static uint8_t       *lh6_payload;
static lha_test_info  lh6_info;

class lh6Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];

        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/lha_lh6.lzh", path);

        lh6_payload = load_lha_payload(filename, &lh6_info);
    }

    void TearDown()
    {
        if(lh6_payload) free(lh6_payload);
    }
};

TEST_F(lh6Fixture, lh6_decompress)
{
    ASSERT_NE(lh6_payload, nullptr);
    EXPECT_EQ(lh6_info.uncompressed_size, (uint32_t)EXPECTED_SIZE);

    size_t   out_len = EXPECTED_SIZE;
    uint8_t *out_buf = (uint8_t *)malloc(out_len);

    int err = lha_decompress_lh6(lh6_payload, lh6_info.payload_size, out_buf, &out_len);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(out_len, (size_t)EXPECTED_SIZE);

    uint32_t crc = crc32_data(out_buf, (uint32_t)out_len);

    free(out_buf);

    EXPECT_EQ(crc, (uint32_t)EXPECTED_CRC32);
}
