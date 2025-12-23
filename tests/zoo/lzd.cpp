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
#include <sys/stat.h>
#include <unistd.h>

#include "../../library.h"
#include "../crc32.h"
#include "gtest/gtest.h"

#define OUTPUT_CHUNK       8192
#define EXPECTED_SIZE      152089
#define EXPECTED_CRC32_VAL 0x66007dba

static long long filesize_of(const char* path)
{
    struct stat st;
    if(stat(path, &st) == 0) return (long long)st.st_size;
    return -1;
}

class lzdFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        char cwd[PATH_MAX];
        char filename[PATH_MAX];

        getcwd(cwd, PATH_MAX);
        snprintf(filename, PATH_MAX, "%s/data/alice29.lzd", cwd);

        inFile = fopen(filename, "rb");
        ASSERT_NE(inFile, nullptr) << "Failed to open input file";

        fileSize = filesize_of(filename);
    }

    void TearDown() override
    {
        if(inFile) fclose(inFile);
    }

    FILE*      inFile   = nullptr;
    long long  fileSize = 0;
};

typedef enum {
    LZD_OK = 0,
    LZD_NEED_INPUT = 1,
    LZD_NEED_OUTPUT = 2,
    LZD_DONE = 3
} LZDStatus;

TEST_F(lzdFixture, ZooMethod1)
{
    unsigned char inbuf[OUTPUT_CHUNK];
    unsigned char outbuf[OUTPUT_CHUNK];

    bool   flushed   = false;
    bool   eof       = false;
    size_t total_out = 0;
    size_t total_in  = 0;
    size_t iter      = 0;

    fprintf(stderr, "INPUT FILE SIZE=%lld bytes\n", fileSize);

    void* ctx = CreateLZDContext();
    ASSERT_NE(ctx, nullptr) << "Failed to create LZD context";

    // allocate buffer for full output to CRC at the end
    uint8_t* full_out = (uint8_t*)malloc(EXPECTED_SIZE);
    ASSERT_NE(full_out, nullptr);
    size_t full_offset = 0;

    while(!eof)
    {
        size_t nread = fread(inbuf, 1, sizeof inbuf, inFile);
        if(nread == 0)
        {
            fprintf(stderr, "[FEED] size=0 flushed=0 (final empty feed)\n");
            LZD_FeedNative(ctx, nullptr, 0);
            flushed = true;
        }
        else
        {
            total_in += nread;
            fprintf(stderr, "[FEED] size=%zu flushed=0 (real data)  total_in=%zu\n",
                    nread, total_in);
            LZD_FeedNative(ctx, inbuf, nread);
        }

        size_t produced = 0;
        int    st       = LZD_OK;
        do
        {
            produced = 0;
            st       = LZD_DrainNative(ctx, outbuf, sizeof outbuf, &produced);
            fprintf(stderr, "-- LOOP iter=%zu total_out=%zu --\n", iter++, total_out);
            fprintf(stderr, "[DRAIN] produced=%zu status=%d flushed=%d eof=%d\n",
                    produced, st, flushed ? 1 : 0, eof ? 1 : 0);

            if(produced > 0)
            {
                memcpy(full_out + full_offset, outbuf, produced);
                full_offset += produced;
                total_out   += produced;
            }
            if(st == LZD_DONE)
            {
                fprintf(stderr, ">>> SET eof=1 (DONE from decoder)\n");
                eof = true;
            }
        } while(produced > 0);

        if(flushed)
        {
            for(int spins = 0; spins < 8 && !eof; spins++)
            {
                size_t more = 0;
                int    st2  = LZD_DrainNative(ctx, outbuf, sizeof outbuf, &more);
                fprintf(stderr, "-- LOOP iter=%zu total_out=%zu --\n", iter++, total_out);
                fprintf(stderr, "[DRAIN] produced=%zu status=%d flushed=1 eof=%d\n",
                        more, st2, eof ? 1 : 0);
                if(more > 0)
                {
                    memcpy(full_out + full_offset, outbuf, more);
                    full_offset += more;
                    total_out   += more;
                }
                else if(st2 == LZD_DONE)
                {
                    fprintf(stderr, ">>> SET eof=1 (DONE after flush)\n");
                    eof = true;
                }
                else
                {
                    break;
                }
            }
            if(!eof)
            {
                fprintf(stderr, ">>> SET eof=1 (no more data and already flushed)\n");
                eof = true;
            }
        }
    }

    fprintf(stderr, "\nTOTAL IN=%zu bytes\n", total_in);
    fprintf(stderr, "TOTAL OUT=%zu bytes\n", total_out);

    DestroyLZDContext(ctx);

    // Now verify the decompressed size and CRC
    EXPECT_EQ(total_out, static_cast<size_t>(EXPECTED_SIZE));
    uint32_t crc = crc32_data(full_out, total_out);
    free(full_out);

    EXPECT_EQ(crc, static_cast<uint32_t>(EXPECTED_CRC32_VAL));
    EXPECT_EQ(total_in, static_cast<size_t>(fileSize));
}
