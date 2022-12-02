/*
 * This file is part of the Aaru Data Preservation Suite.
 * Copyright (c) 2019-2023 Natalia Portillo.
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

#include <stddef.h>
#include <stdint.h>

#include "library.h"
#include "3rdparty/bzip2/bzlib.h"
#include "3rdparty/lzfse/src/lzfse.h"
#include "3rdparty/lzma-21.03beta/C/LzmaLib.h"
#include "3rdparty/zstd-1.5.0/lib/zstd.h"

AARU_EXPORT int32_t AARU_CALL AARU_bzip2_decode_buffer(uint8_t*       dst_buffer,
                                                       uint32_t*      dst_size,
                                                       const uint8_t* src_buffer,
                                                       uint32_t       src_size)
{
    return BZ2_bzBuffToBuffDecompress((char*)dst_buffer, dst_size, (char*)src_buffer, src_size, 0, 0);
}

AARU_EXPORT int32_t AARU_CALL AARU_bzip2_encode_buffer(uint8_t*       dst_buffer,
                                                       uint32_t*      dst_size,
                                                       const uint8_t* src_buffer,
                                                       uint32_t       src_size,
                                                       int32_t        blockSize100k)
{
    return BZ2_bzBuffToBuffCompress((char*)dst_buffer, dst_size, (char*)src_buffer, src_size, blockSize100k, 0, 0);
}

AARU_EXPORT size_t AARU_CALL AARU_lzfse_decode_buffer(uint8_t*       dst_buffer,
                                                      size_t         dst_size,
                                                      const uint8_t* src_buffer,
                                                      size_t         src_size,
                                                      void*          scratch_buffer)
{
    return lzfse_decode_buffer(dst_buffer, dst_size, src_buffer, src_size, scratch_buffer);
}
AARU_EXPORT size_t AARU_CALL AARU_lzfse_encode_buffer(uint8_t*       dst_buffer,
                                                      size_t         dst_size,
                                                      const uint8_t* src_buffer,
                                                      size_t         src_size,
                                                      void*          scratch_buffer)
{
    return lzfse_encode_buffer(dst_buffer, dst_size, src_buffer, src_size, scratch_buffer);
}

AARU_EXPORT int32_t AARU_CALL AARU_lzma_decode_buffer(uint8_t*       dst_buffer,
                                                      size_t*        dst_size,
                                                      const uint8_t* src_buffer,
                                                      size_t*        srcLen,
                                                      const uint8_t* props,
                                                      size_t         propsSize)
{
    return LzmaUncompress(dst_buffer, dst_size, src_buffer, srcLen, props, propsSize);
}

AARU_EXPORT int32_t AARU_CALL AARU_lzma_encode_buffer(uint8_t*       dst_buffer,
                                                      size_t*        dst_size,
                                                      const uint8_t* src_buffer,
                                                      size_t         srcLen,
                                                      uint8_t*       outProps,
                                                      size_t*        outPropsSize,
                                                      int32_t        level,
                                                      uint32_t       dictSize,
                                                      int32_t        lc,
                                                      int32_t        lp,
                                                      int32_t        pb,
                                                      int32_t        fb,
                                                      int32_t        numThreads)
{
    return LzmaCompress(
        dst_buffer, dst_size, src_buffer, srcLen, outProps, outPropsSize, level, dictSize, lc, lp, pb, fb, numThreads);
}

AARU_EXPORT size_t AARU_CALL AARU_zstd_decode_buffer(void*       dst_buffer,
                                                     size_t      dst_size,
                                                     const void* src_buffer,
                                                     size_t      src_size)
{
    return ZSTD_decompress(dst_buffer, dst_size, src_buffer, src_size);
}

AARU_EXPORT size_t AARU_CALL AARU_zstd_encode_buffer(void*       dst_buffer,
                                                     size_t      dst_size,
                                                     const void* src_buffer,
                                                     size_t      src_size,
                                                     int32_t     compressionLevel)
{
    return ZSTD_compress(dst_buffer, dst_size, src_buffer, src_size, compressionLevel);
}

// This is required if BZ_NO_STDIO
void bz_internal_error ( int errcode ) { }

AARU_EXPORT uint64_t AARU_CALL AARU_get_acn_version() { return AARU_CHECKUMS_NATIVE_VERSION; }