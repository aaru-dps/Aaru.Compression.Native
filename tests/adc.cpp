/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2021 Natalia Portillo.
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
#include <cstdint>
#include <cstring>

#include "../adc.h"
#include "../library.h"
#include "crc32.h"
#include "gtest/gtest.h"

#define EXPECTED_CRC32 0x5a5a7388

static const uint8_t* buffer;

class adcFixture : public ::testing::Test
{
  public:
    adcFixture()
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
        snprintf(filename, PATH_MAX, "%s/data/adc.bin", path);

        FILE* file = fopen(filename, "rb");
        buffer     = (const uint8_t*)malloc(34367);
        fread((void*)buffer, 1, 34367, file);
        fclose(file);
    }

    void TearDown() { free((void*)buffer); }

    ~adcFixture()
    {
        // resources cleanup, no exceptions allowed
    }

    // shared user data
};

TEST_F(adcFixture, adc)
{
    auto* outBuf = (uint8_t*)malloc(327680);

    auto decoded = AARU_adc_decode_buffer(outBuf, 327680, buffer, 34367);

    EXPECT_EQ(decoded, 262144);

    auto crc = crc32_data(outBuf, 262144);

    free(outBuf);

    EXPECT_EQ(crc, EXPECTED_CRC32);
}
