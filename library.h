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

#ifndef AARU_COMPRESSION_NATIVE_LIBRARY_H
#define AARU_COMPRESSION_NATIVE_LIBRARY_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#if defined(_WIN32)
#define AARU_CALL   __stdcall
#define AARU_EXPORT EXTERNC __declspec(dllexport)
#define AARU_LOCAL
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif
#else
#define AARU_CALL
#if defined(__APPLE__)
#define AARU_EXPORT EXTERNC __attribute__((visibility("default")))
#define AARU_LOCAL  __attribute__((visibility("hidden")))
#else
#if __GNUC__ >= 4
#define AARU_EXPORT EXTERNC __attribute__((visibility("default")))
#define AARU_LOCAL  __attribute__((visibility("hidden")))
#else
#define AARU_EXPORT EXTERNC
#define AARU_LOCAL
#endif
#endif
#endif

#ifdef _MSC_VER
#define FORCE_INLINE static inline
#else
#define FORCE_INLINE static inline __attribute__((always_inline))
#endif

AARU_EXPORT int32_t AARU_CALL AARU_adc_decode_buffer(uint8_t *dst_buffer, int32_t dst_size, const uint8_t *src_buffer,
                                                     int32_t src_size);

AARU_EXPORT int32_t AARU_CALL AARU_apple_rle_decode_buffer(uint8_t *dst_buffer, int32_t dst_size,
                                                           const uint8_t *src_buffer, int32_t src_size);

// Apple LZH decompression (DART "best" mode: LH1 variant with zero-filled window tail)
AARU_EXPORT int AARU_CALL AARU_apple_lzh_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                       size_t src_size);

AARU_EXPORT size_t AARU_CALL AARU_flac_decode_redbook_buffer(uint8_t *dst_buffer, size_t dst_size,
                                                             const uint8_t *src_buffer, size_t src_size);

AARU_EXPORT size_t AARU_CALL AARU_flac_encode_redbook_buffer(
    uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size, uint32_t blocksize,
    int32_t do_mid_side_stereo, int32_t loose_mid_side_stereo, const char *apodization, uint32_t max_lpc_order,
    uint32_t qlp_coeff_precision, int32_t do_qlp_coeff_prec_search, int32_t do_exhaustive_model_search,
    uint32_t min_residual_partition_order, uint32_t max_residual_partition_order, const char *application_id,
    uint32_t application_id_len);

AARU_EXPORT int32_t AARU_CALL AARU_lz4_decode_buffer(uint8_t *dst_buffer, int32_t dst_size, const uint8_t *src_buffer,
                                                     int32_t src_size);

AARU_EXPORT int32_t AARU_CALL AARU_lz4_encode_buffer(uint8_t *dst_buffer, int32_t dst_size, const uint8_t *src_buffer,
                                                     int32_t src_size);

AARU_EXPORT int32_t AARU_CALL AARU_lzip_decode_buffer(uint8_t *dst_buffer, int32_t dst_size, const uint8_t *src_buffer,
                                                      int32_t src_size);

AARU_EXPORT int32_t AARU_CALL AARU_lzip_encode_buffer(uint8_t *dst_buffer, int32_t dst_size, const uint8_t *src_buffer,
                                                      int32_t src_size, int32_t dictionary_size,
                                                      int32_t match_len_limit);

AARU_EXPORT int32_t AARU_CALL AARU_bzip2_decode_buffer(uint8_t *dst_buffer, uint32_t *dst_size,
                                                       const uint8_t *src_buffer, uint32_t src_size);

AARU_EXPORT int32_t AARU_CALL AARU_bzip2_encode_buffer(uint8_t *dst_buffer, uint32_t *dst_size,
                                                       const uint8_t *src_buffer, uint32_t src_size,
                                                       int32_t blockSize100k);

AARU_EXPORT size_t AARU_CALL AARU_lzfse_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                      size_t src_size, void *scratch_buffer);

AARU_EXPORT size_t AARU_CALL AARU_lzfse_encode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                      size_t src_size, void *scratch_buffer);

AARU_EXPORT size_t AARU_CALL AARU_lzvn_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                     size_t src_size);

AARU_EXPORT size_t AARU_CALL AARU_lzvn_encode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                     size_t src_size, void *scratch_buffer);

AARU_EXPORT int32_t AARU_CALL AARU_lzma_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                      size_t *src_size, const uint8_t *props, size_t propsSize);

AARU_EXPORT int32_t AARU_CALL AARU_lzma_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                      size_t src_size, uint8_t *outProps, size_t *outPropsSize,
                                                      int32_t level, uint32_t dictSize, int32_t lc, int32_t lp,
                                                      int32_t pb, int32_t fb, int32_t numThreads);

AARU_EXPORT int32_t AARU_CALL AARU_xz_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size);

AARU_EXPORT int32_t AARU_CALL AARU_xz_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size, uint32_t preset, uint32_t checkType);

AARU_EXPORT size_t AARU_CALL AARU_zstd_decode_buffer(void *dst_buffer, size_t dst_size, const void *src_buffer,
                                                     size_t src_size);

AARU_EXPORT size_t AARU_CALL AARU_zstd_encode_buffer(void *dst_buffer, size_t dst_size, const void *src_buffer,
                                                     size_t src_size, int32_t compressionLevel);

/**
 * LZO Algorithm Types
 */
typedef enum
{
    AARU_LZO_ALGORITHM_LZO1  = 0, /* LZO1 algorithm */
    AARU_LZO_ALGORITHM_LZO1A = 1, /* LZO1A algorithm */
    AARU_LZO_ALGORITHM_LZO1B = 2, /* LZO1B algorithm (supports compression levels 1-9, 99, 999) */
    AARU_LZO_ALGORITHM_LZO1C = 3, /* LZO1C algorithm (supports compression levels 1-9, 99, 999) */
    AARU_LZO_ALGORITHM_LZO1F = 4, /* LZO1F algorithm (supports compression level 999) */
    AARU_LZO_ALGORITHM_LZO1X = 5, /* LZO1X algorithm (supports compression levels 11, 12, 15, 999) - most common */
    AARU_LZO_ALGORITHM_LZO1Y = 6, /* LZO1Y algorithm (supports compression level 999) */
    AARU_LZO_ALGORITHM_LZO1Z = 7, /* LZO1Z algorithm (only 999 compression level) */
    AARU_LZO_ALGORITHM_LZO2A = 8  /* LZO2A algorithm (only 999 compression level) */
} aaru_lzo_algorithm_t;

AARU_EXPORT int32_t AARU_CALL AARU_lzo_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                     size_t src_size, int32_t algorithm);

AARU_EXPORT int32_t AARU_CALL AARU_lzo_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                     size_t src_size, int32_t algorithm, int32_t compression_level);

AARU_EXPORT void AARU_CALL *CreateLZDContext(void);

AARU_EXPORT void AARU_CALL DestroyLZDContext(void *ctx);

AARU_EXPORT int AARU_CALL LZD_FeedNative(void *ctx, const unsigned char *data, size_t length);

AARU_EXPORT int AARU_CALL LZD_DrainNative(void *ctx, unsigned char *outBuf, size_t outBufLen, size_t *produced);

AARU_EXPORT int AARU_CALL lh5_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

#define AARU_CHECKUMS_NATIVE_VERSION 0x06000089

AARU_EXPORT uint64_t AARU_CALL AARU_get_acn_version();

// ARC method 3: Stored with non-repeat packing
AARU_EXPORT int AARU_CALL arc_decompress_pack(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                              size_t *out_len);
// ARC method 4: Huffman squeezing
AARU_EXPORT int AARU_CALL arc_decompress_squeeze(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                 size_t *out_len);
// ARC Method 5: LZW (crunching)
AARU_EXPORT int AARU_CALL arc_decompress_crunch(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                size_t *out_len);
// ARC Method 6: LZW with non-repeat packing (crunching)
AARU_EXPORT int AARU_CALL arc_decompress_crunch_nrpack(const unsigned char *in_buf, size_t in_len,
                                                       unsigned char *out_buf, size_t *out_len);
// ARC Method 7: LZW with non-repeat packing and new hash (Crunching)
AARU_EXPORT int AARU_CALL arc_decompress_crunch_nrpack_new(const unsigned char *in_buf, size_t in_len,
                                                           unsigned char *out_buf, size_t *out_len);

// ARC Method 8: Dynamic LZW (crunching)
AARU_EXPORT int AARU_CALL arc_decompress_crunch_dynamic(const unsigned char *in_buf, size_t in_len,
                                                        unsigned char *out_buf, size_t *out_len);

// ARC Method 9: Dynamic LZW with 13 bits (squashing)
AARU_EXPORT int AARU_CALL arc_decompress_squash(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                size_t *out_len);

// ARC/PAK Method 10: LZW (crush) (unsure why it's different of the others but even XADMaster uses different codepaths)
AARU_EXPORT int AARU_CALL pak_decompress_crush(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                               size_t *out_len);

// ARC/PAK Method 11: LZSS (distill)
AARU_EXPORT int AARU_CALL pak_decompress_distill(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                                 size_t *out_len);

/**
 * HA Algorithm Types
 */
typedef enum
{
    HA_ALGORITHM_ASC = 0, /* ASC algorithm */
    HA_ALGORITHM_HSC = 1  /* HSC algorithm */
} ha_algorithm_t;

AARU_EXPORT int AARU_CALL ha_asc_decompress(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                            size_t *out_len);

AARU_EXPORT int AARU_CALL ha_hsc_decompress(const unsigned char *in_buf, size_t in_len, unsigned char *out_buf,
                                            size_t *out_len);

// LHA -lh1- (Dynamic Huffman, 4KB window)
AARU_EXPORT int AARU_CALL lha_decompress_lh1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LHA -lh2- (Dynamic Huffman, legacy)
AARU_EXPORT int AARU_CALL lha_decompress_lh2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LHA -lh3- (Static Huffman, legacy)
AARU_EXPORT int AARU_CALL lha_decompress_lh3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LHA -lh4- (Block Huffman, 4KB window)
AARU_EXPORT int AARU_CALL lha_decompress_lh4(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LHA -lh5- (Block Huffman, 8KB window)
AARU_EXPORT int AARU_CALL lha_decompress_lh5(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LHA -lh6- (Block Huffman, 32KB window)
AARU_EXPORT int AARU_CALL lha_decompress_lh6(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LHA -lh7- (Block Huffman, 64KB window)
AARU_EXPORT int AARU_CALL lha_decompress_lh7(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LArc -lzs- (LZSS, 2KB window)
AARU_EXPORT int AARU_CALL larc_decompress_lzs(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// LArc -lz5- (Flag-byte LZSS, 4KB window)
AARU_EXPORT int AARU_CALL larc_decompress_lz5(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// PMarc -pm1- (Adaptive history + Huffman)
AARU_EXPORT int AARU_CALL pmarc_decompress_pm1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// PMarc -pm2- (Dynamic trees + history, legacy)
AARU_EXPORT int AARU_CALL pmarc_decompress_pm2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// ACE v1 (LZ77) decompression
AARU_EXPORT int AARU_CALL ace_decompress_lz77(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len,
                                              int dic_bits);

// ACE v2 (Blocked) decompression
AARU_EXPORT int AARU_CALL ace_decompress_blocked(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len, int dic_bits);

// ARJ Method 1 (LZH, most compression)
AARU_EXPORT int AARU_CALL arj_decompress_method1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

// ARJ Method 2 (LZH, medium compression)
AARU_EXPORT int AARU_CALL arj_decompress_method2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

// ARJ Method 3 (LZH, fast compression)
AARU_EXPORT int AARU_CALL arj_decompress_method3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

// ARJ Method 4 (Fastest, variable-width LZSS)
AARU_EXPORT int AARU_CALL arj_decompress_fastest(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len);

// ARJZ Method 1 (LZH, 64KB window)
AARU_EXPORT int AARU_CALL arjz_decompress_method1(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len);

// ARJZ Method 2 (LZH, 64KB window)
AARU_EXPORT int AARU_CALL arjz_decompress_method2(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len);

// ARJZ Method 3 (LZH, 64KB window)
AARU_EXPORT int AARU_CALL arjz_decompress_method3(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                  size_t *out_len);

// ARJZ custom extended DEFLATE decompression
AARU_EXPORT int AARU_CALL arjz_decompress_buffer(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf,
                                                 size_t *out_len, size_t orig_size);

// LZMA2 decode (single prop byte instead of 5-byte props blob)
AARU_EXPORT int32_t AARU_CALL AARU_lzma2_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                       size_t *src_size, uint8_t prop);

// LZMA2 encode
AARU_EXPORT int32_t AARU_CALL AARU_lzma2_encode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                       size_t src_size, uint8_t *outProp, int32_t level,
                                                       uint32_t dictSize, int32_t lc, int32_t lp, int32_t pb,
                                                       int32_t fb, int32_t numThreads);

// ZIP method 1: Shrink (LZW, 9-13 bit codes)
AARU_EXPORT int AARU_CALL AARU_zip_shrink_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                        const uint8_t *src_buffer, size_t src_size);

// ZIP methods 2-5: Reduce (follower sets + LZ77, compression factor 1-4)
AARU_EXPORT int AARU_CALL AARU_zip_reduce_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                        const uint8_t *src_buffer, size_t src_size, int comp_factor);

// ZIP method 6: Implode (Shannon-Fano + LZSS)
AARU_EXPORT int AARU_CALL AARU_zip_implode_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                         const uint8_t *src_buffer, size_t src_size,
                                                         int large_dictionary, int has_literals);

// ZIP method 9: Deflate64
AARU_EXPORT int AARU_CALL AARU_zip_deflate64_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                           const uint8_t *src_buffer, size_t src_size);

// ZIP method 98: PPMd variant I
AARU_EXPORT int AARU_CALL AARU_zip_ppmd_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer,
                                                      size_t src_size, int max_order, int sub_alloc_size,
                                                      int restoration);

// ZIP method 97: WinZip WavPack
AARU_EXPORT int AARU_CALL AARU_zip_wavpack_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                         const uint8_t *src_buffer, size_t src_size,
                                                         uint32_t num_samples, int bits_per_sample, int num_channels);

// ZIP method 96: WinZip JPEG
AARU_EXPORT int AARU_CALL AARU_zip_winzipjpeg_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                            const uint8_t *src_buffer, size_t src_size);

// RAR 1.5 (UNP_VER=15): Custom LZ77 with fixed Huffman tables, 64KB window
AARU_EXPORT int AARU_CALL rar15_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// RAR 2.0 (UNP_VER=20): Block Huffman LZ77 with optional audio, 1MB window
AARU_EXPORT int AARU_CALL rar20_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// RAR 3.0 (UNP_VER=29): Huffman LZ77 / PPMd Variant H + VM filters, 4MB window
AARU_EXPORT int AARU_CALL rar30_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

// RAR 5.0: Huffman LZ77 + native filters, variable window
AARU_EXPORT int AARU_CALL rar50_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len,
                                           size_t window_size);

// Compact Pro: RLE decompression (always applied)
AARU_EXPORT int AARU_CALL AARU_cpt_rle_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                     size_t src_size);

// Compact Pro: LZH + RLE decompression (block Huffman LZSS + RLE)
AARU_EXPORT int AARU_CALL AARU_cpt_lzh_rle_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                         const uint8_t *src_buffer, size_t src_size);

// DiskDoubler: ADn block LZSS decompression (methods 6 and 9)
AARU_EXPORT int AARU_CALL AARU_dd_adn_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size);

// DiskDoubler: DDn block Huffman LZ77 decompression (method 10)
AARU_EXPORT int AARU_CALL AARU_dd_ddn_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size);

// DiskDoubler: Method 2 adaptive Huffman decompression (methods 2 and 5)
AARU_EXPORT int AARU_CALL AARU_dd_method2_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                        const uint8_t *src_buffer, size_t src_size, int num_trees);

// DiskDoubler: Stac LZS decompression (method 7)
AARU_EXPORT int AARU_CALL AARU_dd_stac_lzs_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                         const uint8_t *src_buffer, size_t src_size);

// DiskDoubler: Compact Pro decompression (method 8)
AARU_EXPORT int AARU_CALL AARU_dd_cpt_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size);

// DiskDoubler: LZW decompression (method 1, Unix compress variant)
AARU_EXPORT int AARU_CALL AARU_dd_lzw_decode_buffer(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer,
                                                    size_t src_size, int flags);

// StuffIt classic method 1: RLE with 0x90 escape
AARU_EXPORT int AARU_CALL AARU_stuffit_rle90_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                           const uint8_t *src_buffer, size_t src_size);

// StuffIt classic method 2: UNIX compress (LZW)
AARU_EXPORT int AARU_CALL AARU_stuffit_compress_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                              const uint8_t *src_buffer, size_t src_size);

// StuffIt classic method 3: Static Huffman tree
AARU_EXPORT int AARU_CALL AARU_stuffit_huffman_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                             const uint8_t *src_buffer, size_t src_size);

// StuffIt classic method 5: LZAH (LZH with adaptive Huffman, 4KB window)
AARU_EXPORT int AARU_CALL AARU_stuffit_lzah_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                          const uint8_t *src_buffer, size_t src_size);

// StuffIt classic method 8: MW (LZW variant)
AARU_EXPORT int AARU_CALL AARU_stuffit_mw_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                        const uint8_t *src_buffer, size_t src_size);

// StuffIt classic method 13: Dynamic/Static Huffman LZSS (64KB window)
AARU_EXPORT int AARU_CALL AARU_stuffit_method13_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                              const uint8_t *src_buffer, size_t src_size);

// StuffIt classic method 14: Block Huffman LZSS (256KB window)
AARU_EXPORT int AARU_CALL AARU_stuffit_method14_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                              const uint8_t *src_buffer, size_t src_size);

// StuffIt classic method 15: Arsenic (BWT + arithmetic coding)
AARU_EXPORT int AARU_CALL AARU_stuffit_arsenic_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                             const uint8_t *src_buffer, size_t src_size);

// StuffIt X method 0: Brimstone (PPMd Variant G)
AARU_EXPORT int AARU_CALL AARU_stuffitx_brimstone_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                                const uint8_t *src_buffer, size_t src_size,
                                                                int max_order, int sub_alloc_size);

// StuffIt X method 1: Cyanide (BWT + ternary range coding)
AARU_EXPORT int AARU_CALL AARU_stuffitx_cyanide_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                              const uint8_t *src_buffer, size_t src_size);

// StuffIt X method 2: Darkhorse (context-weighted LZSS + range coder)
AARU_EXPORT int AARU_CALL AARU_stuffitx_darkhorse_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                                const uint8_t *src_buffer, size_t src_size,
                                                                int window_bits);

// StuffIt X method 3: Modified Deflate
AARU_EXPORT int AARU_CALL AARU_stuffitx_deflate_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                              const uint8_t *src_buffer, size_t src_size);

// StuffIt X method 4: Blend (Darkhorse/Cyanide/Brimstone multiplexer)
AARU_EXPORT int AARU_CALL AARU_stuffitx_blend_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                            const uint8_t *src_buffer, size_t src_size);

// StuffIt X method 6: Iron (advanced BWT/ST4 + range coder)
AARU_EXPORT int AARU_CALL AARU_stuffitx_iron_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                           const uint8_t *src_buffer, size_t src_size);

// StuffIt X preprocessor 0: English dictionary word substitution
AARU_EXPORT int AARU_CALL AARU_stuffitx_english_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                              const uint8_t *src_buffer, size_t src_size);

// StuffIt X preprocessor 2: x86 executable address transformation
AARU_EXPORT int AARU_CALL AARU_stuffitx_x86_decode_buffer(uint8_t *dst_buffer, size_t *dst_size,
                                                          const uint8_t *src_buffer, size_t src_size);

#endif  // AARU_COMPRESSION_NATIVE_LIBRARY_H
