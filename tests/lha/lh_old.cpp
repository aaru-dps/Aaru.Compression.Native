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
#include "../../lha/lh_old.h"
#include "../crc32.h"
#include "gtest/gtest.h"
#include "lha_helpers.h"

/* PMarc -pm2- decompresses to 152192 bytes */
#define PM2_EXPECTED_SIZE  152192
#define PM2_EXPECTED_CRC32 0x1bbf031e

static uint8_t       *pm2_payload;
static lha_test_info  pm2_info;

class pm2Fixture : public ::testing::Test
{
protected:
    void SetUp()
    {
        char path[PATH_MAX];
        char filename[PATH_MAX];

        getcwd(path, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/pmarc_pm2.pma", path);

        pm2_payload = load_lha_payload(filename, &pm2_info);
    }

    void TearDown()
    {
        if(pm2_payload) free(pm2_payload);
    }
};

TEST_F(pm2Fixture, pm2_decompress)
{
    ASSERT_NE(pm2_payload, nullptr);
    EXPECT_EQ(pm2_info.uncompressed_size, (uint32_t)PM2_EXPECTED_SIZE);

    size_t   out_len = PM2_EXPECTED_SIZE;
    uint8_t *out_buf = (uint8_t *)malloc(out_len);

    int err = pmarc_decompress_pm2(pm2_payload, pm2_info.payload_size, out_buf, &out_len);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(out_len, (size_t)PM2_EXPECTED_SIZE);

    uint32_t crc = crc32_data(out_buf, (uint32_t)out_len);

    free(out_buf);

    /* Validate CRC32 of decompressed data */
    EXPECT_EQ(crc, (uint32_t)PM2_EXPECTED_CRC32);
}
