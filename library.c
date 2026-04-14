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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "library.h"
#include "3rdparty/bzip2/bzlib.h"
#include "3rdparty/lz4/lib/lz4.h"
#include "3rdparty/lzfse/src/lzfse.h"
#include "3rdparty/lzfse/src/lzvn_decode_base.h"
#include "3rdparty/lzfse/src/lzvn_encode_base.h"
#include "3rdparty/lzma/C/7zCrc.h"
#include "3rdparty/lzma/C/Alloc.h"
#include "3rdparty/lzma/C/Lzma2Dec.h"
#include "3rdparty/lzma/C/Lzma2Enc.h"
#include "3rdparty/lzma/C/LzmaLib.h"
#include "3rdparty/lzma/C/Xz.h"
#include "3rdparty/lzma/C/XzCrc64.h"
#include "3rdparty/lzma/C/XzEnc.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1a.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1b.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1c.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1f.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1x.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1y.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo1z.h"
#include "3rdparty/lzo-2.10/include/lzo/lzo2a.h"
#include "3rdparty/lzo-2.10/include/lzo/lzoconf.h"
#include "3rdparty/lzo-2.10/include/lzo/lzodefs.h"
#include "3rdparty/zstd/lib/zstd.h"
#include "ace/ace.h"
#include "zip/zip.h"

AARU_EXPORT int32_t AARU_CALL AARU_bzip2_decode_buffer(uint8_t *dst_buffer, uint32_t *dst_size,
                                                       const uint8_t *src_buffer, uint32_t src_size)
{ return BZ2_bzBuffToBuffDecompress((char *)dst_buffer, dst_size, (char *)src_buffer, src_size, 0, 0); }

AARU_EXPORT int32_t AARU_CALL AARU_bzip2_encode_buffer(uint8_t *dst_buffer, uint32_t *dst_size,
                                                       const uint8_t *src_buffer, uint32_t src_size,
                                                       int32_t blockSize100k)
{ return BZ2_bzBuffToBuffCompress((char *)dst_buffer, dst_size, (char *)src_buffer, src_size, blockSize100k, 0, 0); }

AARU_EXPORT int32_t AARU_CALL AARU_lz4_decode_buffer(uint8_t *dst_buffer, int32_t dst_size, const uint8_t *src_buffer,
                                                     int32_t src_size)
{ return LZ4_decompress_safe((const char *)src_buffer, (char *)dst_buffer, src_size, dst_size); }

AARU_EXPORT int32_t AARU_CALL AARU_lz4_encode_buffer(uint8_t *dst_buffer, int32_t dst_size, const uint8_t *src_buffer,
                                                     int32_t src_size)
{ return LZ4_compress_default((const char *)src_buffer, (char *)dst_buffer, src_size, dst_size); }

AARU_EXPORT size_t AARU_CALL AARU_lzfse_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                      size_t src_size, void *scratch_buffer)
{ return lzfse_decode_buffer(dst_buffer, dst_size, src_buffer, src_size, scratch_buffer); }

AARU_EXPORT size_t AARU_CALL AARU_lzfse_encode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                      size_t src_size, void *scratch_buffer)
{ return lzfse_encode_buffer(dst_buffer, dst_size, src_buffer, src_size, scratch_buffer); }

AARU_EXPORT size_t AARU_CALL AARU_lzvn_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                     size_t src_size)
{
    lzvn_decoder_state state;
    memset(&state, 0, sizeof(state));

    state.src     = src_buffer;
    state.src_end = src_buffer + src_size;

    state.dst       = dst_buffer;
    state.dst_begin = dst_buffer;
    state.dst_end   = dst_buffer + dst_size;

    lzvn_decode(&state);

    return (size_t)(state.dst - dst_buffer);
}

AARU_EXPORT size_t AARU_CALL AARU_lzvn_encode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                     size_t src_size, void *scratch_buffer)
{
    lzvn_encoder_state state;
    memset(&state, 0, sizeof(state));

    state.src             = src_buffer;
    state.src_begin       = 0;
    state.src_end         = (lzvn_offset)src_size;
    state.src_current     = 0;
    state.src_current_end = (lzvn_offset)src_size - LZVN_ENCODE_MIN_MARGIN;
    state.src_literal     = 0;

    state.dst       = dst_buffer;
    state.dst_begin = dst_buffer;
    state.dst_end   = dst_buffer + dst_size;

    state.table = (lzvn_encode_entry_type *)scratch_buffer;

    lzvn_encode(&state);

    return (size_t)(state.dst - dst_buffer);
}

AARU_EXPORT int32_t AARU_CALL AARU_lzma_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                      size_t *srcLen, const uint8_t *props, size_t propsSize)
{ return LzmaUncompress(dst_buffer, dst_size, src_buffer, srcLen, props, propsSize); }

AARU_EXPORT int32_t AARU_CALL AARU_lzma_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                      size_t srcLen, uint8_t *outProps, size_t *outPropsSize,
                                                      int32_t level, uint32_t dictSize, int32_t lc, int32_t lp,
                                                      int32_t pb, int32_t fb, int32_t numThreads)
{
    return LzmaCompress(dst_buffer, dst_size, src_buffer, srcLen, outProps, outPropsSize, level, dictSize, lc, lp, pb,
                        fb, numThreads);
}

// XZ buffer stream structures
typedef struct
{
    ISeqInStream vt;
    const Byte  *data;
    size_t       size;
    size_t       pos;
} CBufferInStream;

static SRes BufferInStream_Read(ISeqInStreamPtr pp, void *buf, size_t *size)
{
    CBufferInStream *p         = Z7_CONTAINER_FROM_VTBL(pp, CBufferInStream, vt);
    size_t           remaining = p->size - p->pos;
    if(*size > remaining) *size = remaining;
    memcpy(buf, p->data + p->pos, *size);
    p->pos += *size;
    return SZ_OK;
}

typedef struct
{
    ISeqOutStream vt;
    Byte         *data;
    size_t        size;
    size_t        pos;
} CBufferOutStream;

static size_t BufferOutStream_Write(ISeqOutStreamPtr pp, const void *buf, size_t size)
{
    CBufferOutStream *p         = Z7_CONTAINER_FROM_VTBL(pp, CBufferOutStream, vt);
    size_t            remaining = p->size - p->pos;
    if(size > remaining) size = remaining;
    memcpy(p->data + p->pos, buf, size);
    p->pos += size;
    return size;
}

static void xz_init_crc_tables(void)
{
    static int initialized = 0;
    if(!initialized)
    {
        CrcGenerateTable();
        Crc64GenerateTable();
        initialized = 1;
    }
}

AARU_EXPORT int32_t AARU_CALL AARU_xz_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size)
{
    CXzUnpacker  state;
    SizeT        destLen = (SizeT)*dst_size;
    SizeT        srcLen  = (SizeT)src_size;
    ECoderStatus status;
    SRes         res;

    xz_init_crc_tables();

    XzUnpacker_Construct(&state, &g_Alloc);
    XzUnpacker_Init(&state);

    res = XzUnpacker_CodeFull(&state, dst_buffer, &destLen, src_buffer, &srcLen, CODER_FINISH_END, &status);

    *dst_size = destLen;

    XzUnpacker_Free(&state);

    return res;
}

AARU_EXPORT int32_t AARU_CALL AARU_xz_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size, uint32_t preset, uint32_t checkType)
{
    CXzProps         props;
    CBufferInStream  inStream;
    CBufferOutStream outStream;
    SRes             res;

    xz_init_crc_tables();

    XzProps_Init(&props);
    props.lzma2Props.lzmaProps.level = preset > 9 ? 9 : preset;
    props.checkId                    = checkType > XZ_CHECK_SHA256 ? XZ_CHECK_CRC64 : checkType;

    inStream.vt.Read = BufferInStream_Read;
    inStream.data    = src_buffer;
    inStream.size    = src_size;
    inStream.pos     = 0;

    outStream.vt.Write = BufferOutStream_Write;
    outStream.data     = dst_buffer;
    outStream.size     = *dst_size;
    outStream.pos      = 0;

    res = Xz_Encode(&outStream.vt, &inStream.vt, &props, NULL);

    *dst_size = outStream.pos;

    return res;
}

AARU_EXPORT size_t AARU_CALL AARU_zstd_decode_buffer(void *dst_buffer, size_t dst_size, const void *src_buffer,
                                                     size_t src_size)
{ return ZSTD_decompress(dst_buffer, dst_size, src_buffer, src_size); }

AARU_EXPORT size_t AARU_CALL AARU_zstd_encode_buffer(void *dst_buffer, size_t dst_size, const void *src_buffer,
                                                     size_t src_size, int32_t compressionLevel)
{ return ZSTD_compress(dst_buffer, dst_size, src_buffer, src_size, compressionLevel); }

AARU_EXPORT int32_t AARU_CALL AARU_lzo_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                     size_t src_size, int32_t algorithm)
{
    lzo_uint out_len = *dst_size;

    int result = lzo_init();
    if(result != LZO_E_OK) return result;  // Initialization failed

    switch(algorithm)
    {
        case AARU_LZO_ALGORITHM_LZO1:
            result = lzo1_decompress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO1A:
            result = lzo1a_decompress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO1B:
            result = lzo1b_decompress_safe(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO1C:
            result = lzo1c_decompress_safe(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO1F:
            result = lzo1f_decompress_safe(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO1X:
            result = lzo1x_decompress_safe(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO1Y:
            result = lzo1y_decompress_safe(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO1Z:
            result = lzo1z_decompress_safe(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        case AARU_LZO_ALGORITHM_LZO2A:
            result = lzo2a_decompress_safe(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, NULL);
            break;
        default:
            return -1;  // Invalid algorithm
    }

    *dst_size = out_len;
    return result;
}

AARU_EXPORT int32_t AARU_CALL AARU_lzo_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                     size_t src_size, int32_t algorithm, int32_t compression_level)
{
    lzo_uint out_len     = *dst_size;
    void    *wrkmem      = NULL;
    size_t   wrkmem_size = 0;

    // Determine work memory size based on algorithm and compression level
    switch(algorithm)
    {
        case AARU_LZO_ALGORITHM_LZO1:
            if(compression_level == 99)
                wrkmem_size = LZO1_99_MEM_COMPRESS;
            else
                wrkmem_size = LZO1_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO1A:
            if(compression_level == 99)
                wrkmem_size = LZO1A_99_MEM_COMPRESS;
            else
                wrkmem_size = LZO1A_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO1B:
            if(compression_level == 99)
                wrkmem_size = LZO1B_99_MEM_COMPRESS;
            else if(compression_level == 999)
                wrkmem_size = LZO1B_999_MEM_COMPRESS;
            else
                wrkmem_size = LZO1B_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO1C:
            if(compression_level == 99)
                wrkmem_size = LZO1C_99_MEM_COMPRESS;
            else if(compression_level == 999)
                wrkmem_size = LZO1C_999_MEM_COMPRESS;
            else
                wrkmem_size = LZO1C_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO1F:
            if(compression_level == 999)
                wrkmem_size = LZO1F_999_MEM_COMPRESS;
            else
                wrkmem_size = LZO1F_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO1X:
            if(compression_level == 11)
                wrkmem_size = LZO1X_1_11_MEM_COMPRESS;
            else if(compression_level == 12)
                wrkmem_size = LZO1X_1_12_MEM_COMPRESS;
            else if(compression_level == 15)
                wrkmem_size = LZO1X_1_15_MEM_COMPRESS;
            else if(compression_level == 999)
                wrkmem_size = LZO1X_999_MEM_COMPRESS;
            else
                wrkmem_size = LZO1X_1_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO1Y:
            if(compression_level == 999)
                wrkmem_size = LZO1Y_999_MEM_COMPRESS;
            else
                wrkmem_size = LZO1Y_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO1Z:
            wrkmem_size = LZO1Z_999_MEM_COMPRESS;
            break;
        case AARU_LZO_ALGORITHM_LZO2A:
            wrkmem_size = LZO2A_999_MEM_COMPRESS;
            break;
        default:
            return -1;  // Invalid algorithm
    }

    int result = lzo_init();
    if(result != LZO_E_OK) return result;  // Initialization failed

    wrkmem = malloc(wrkmem_size);
    if(wrkmem == NULL) return -1;

    // Call the appropriate compression function
    switch(algorithm)
    {
        case AARU_LZO_ALGORITHM_LZO1:
            if(compression_level == 99)
                result = lzo1_99_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else
                result = lzo1_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            break;
        case AARU_LZO_ALGORITHM_LZO1A:
            if(compression_level == 99)
                result = lzo1a_99_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else
                result = lzo1a_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            break;
        case AARU_LZO_ALGORITHM_LZO1B:
            if(compression_level == 99)
                result = lzo1b_99_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else if(compression_level == 999)
                result = lzo1b_999_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else if(compression_level >= 1 && compression_level <= 9)
                result =
                    lzo1b_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem, compression_level);
            else
                result = lzo1b_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem,
                                        LZO1B_DEFAULT_COMPRESSION);
            break;
        case AARU_LZO_ALGORITHM_LZO1C:
            if(compression_level == 99)
                result = lzo1c_99_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else if(compression_level == 999)
                result = lzo1c_999_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else if(compression_level >= 1 && compression_level <= 9)
                result =
                    lzo1c_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem, compression_level);
            else
                result = lzo1c_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem,
                                        LZO1C_DEFAULT_COMPRESSION);
            break;
        case AARU_LZO_ALGORITHM_LZO1F:
            if(compression_level == 999)
                result = lzo1f_999_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else
                result = lzo1f_1_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            break;
        case AARU_LZO_ALGORITHM_LZO1X:
            if(compression_level == 11)
                result = lzo1x_1_11_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else if(compression_level == 12)
                result = lzo1x_1_12_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else if(compression_level == 15)
                result = lzo1x_1_15_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else if(compression_level == 999)
                result = lzo1x_999_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else
                result = lzo1x_1_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            break;
        case AARU_LZO_ALGORITHM_LZO1Y:
            if(compression_level == 999)
                result = lzo1y_999_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            else
                result = lzo1y_1_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            break;
        case AARU_LZO_ALGORITHM_LZO1Z:
            result = lzo1z_999_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            break;
        case AARU_LZO_ALGORITHM_LZO2A:
            result = lzo2a_999_compress(src_buffer, (lzo_uint)src_size, dst_buffer, &out_len, wrkmem);
            break;
        default:
            free(wrkmem);
            return -1;  // Invalid algorithm
    }

    free(wrkmem);
    *dst_size = out_len;
    return result;
}

// This is required if BZ_NO_STDIO
void bz_internal_error(int errcode) {}

AARU_EXPORT int AARU_CALL ace_decompress_lz77(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len,
                                              int dic_bits)
{
    ace_decompress_ctx_t ctx;
    int                  ret;

    if(ace_decompress_init(&ctx, dic_bits) != 0) return -1;

    ret = ace_decompress_v1(&ctx, in_buf, in_len, out_buf, out_len);
    ace_decompress_free(&ctx);

    return ret;
}

AARU_EXPORT int AARU_CALL ace_decompress_blocked(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len, int dic_bits)
{
    ace_decompress_ctx_t ctx;
    int                  ret;

    if(ace_decompress_init(&ctx, dic_bits) != 0) return -1;

    ret = ace_decompress_v2(&ctx, in_buf, in_len, out_buf, out_len);
    ace_decompress_free(&ctx);

    return ret;
}

/* ============== LZMA2 ============== */

AARU_EXPORT int32_t AARU_CALL AARU_lzma2_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                       size_t *src_size, uint8_t prop)
{
    ELzmaStatus status;
    return Lzma2Decode(dst_buffer, (SizeT *)dst_size, src_buffer, (SizeT *)src_size, prop, LZMA_FINISH_END, &status,
                       &g_Alloc);
}

AARU_EXPORT int32_t AARU_CALL AARU_lzma2_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                       size_t src_size, uint8_t *outProp, int32_t level,
                                                       uint32_t dictSize, int32_t lc, int32_t lp, int32_t pb,
                                                       int32_t fb, int32_t numThreads)
{
    CLzma2EncHandle enc;
    CLzma2EncProps  props;
    SRes            res;

    enc = Lzma2Enc_Create(&g_Alloc, &g_Alloc);
    if(!enc) return SZ_ERROR_MEM;

    Lzma2EncProps_Init(&props);
    props.lzmaProps.level      = level;
    props.lzmaProps.dictSize   = dictSize;
    props.lzmaProps.lc         = lc;
    props.lzmaProps.lp         = lp;
    props.lzmaProps.pb         = pb;
    props.lzmaProps.fb         = fb;
    props.lzmaProps.numThreads = numThreads;

    res = Lzma2Enc_SetProps(enc, &props);
    if(res != SZ_OK)
    {
        Lzma2Enc_Destroy(enc);
        return res;
    }

    *outProp = Lzma2Enc_WriteProperties(enc);

    res = Lzma2Enc_Encode2(enc, NULL, dst_buffer, dst_size, NULL, src_buffer, src_size, NULL);

    Lzma2Enc_Destroy(enc);
    return res;
}

/* ============== ZIP Wrappers ============== */

AARU_EXPORT int AARU_CALL AARU_zip_shrink_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                        const uint8_t *src_buffer, size_t src_size)
{ return zip_shrink_decompress(src_buffer, src_size, dst_buffer, dst_size); }

AARU_EXPORT int AARU_CALL AARU_zip_reduce_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                        const uint8_t *src_buffer, size_t src_size, int comp_factor)
{ return zip_reduce_decompress(src_buffer, src_size, dst_buffer, dst_size, comp_factor); }

AARU_EXPORT int AARU_CALL AARU_zip_implode_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                         const uint8_t *src_buffer, size_t src_size,
                                                         int large_dictionary, int has_literals)
{ return zip_implode_decompress(src_buffer, src_size, dst_buffer, dst_size, large_dictionary, has_literals); }

AARU_EXPORT int AARU_CALL AARU_zip_deflate64_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                           const uint8_t *src_buffer, size_t src_size)
{ return zip_deflate64_decompress(src_buffer, src_size, dst_buffer, dst_size); }

AARU_EXPORT int AARU_CALL AARU_zip_ppmd_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                      size_t src_size, int max_order, int sub_alloc_size,
                                                      int restoration)
{ return zip_ppmd_decompress(dst_buffer, dst_size, src_buffer, src_size, max_order, sub_alloc_size, restoration); }

AARU_EXPORT int AARU_CALL AARU_zip_wavpack_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                         const uint8_t *src_buffer, size_t src_size,
                                                         uint32_t num_samples, int bits_per_sample, int num_channels)
{
    return zip_wavpack_decompress(dst_buffer, dst_size, src_buffer, src_size, num_samples, bits_per_sample,
                                  num_channels);
}

AARU_EXPORT int AARU_CALL AARU_zip_winzipjpeg_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                            const uint8_t *src_buffer, size_t src_size)
{ return zip_winzipjpeg_decompress(dst_buffer, dst_size, src_buffer, src_size); }

AARU_EXPORT uint64_t AARU_CALL AARU_get_acn_version() { return AARU_CHECKUMS_NATIVE_VERSION; }