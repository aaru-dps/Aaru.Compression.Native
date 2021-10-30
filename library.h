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

#ifndef AARU_COMPRESSION_NATIVE_LIBRARY_H
#define AARU_COMPRESSION_NATIVE_LIBRARY_H

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#if defined(_WIN32)
#define AARU_CALL __stdcall
#define AARU_EXPORT EXTERNC __declspec(dllexport)
#define AARU_LOCAL
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif
#else
#define AARU_CALL
#if defined(__APPLE__)
#define AARU_EXPORT EXTERNC __attribute__((visibility("default")))
#define AARU_LOCAL __attribute__((visibility("hidden")))
#else
#if __GNUC__ >= 4
#define AARU_EXPORT EXTERNC __attribute__((visibility("default")))
#define AARU_LOCAL __attribute__((visibility("hidden")))
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

AARU_EXPORT int32_t AARU_CALL adc_decode_buffer(uint8_t*       dst_buffer,
                                                int32_t        dst_size,
                                                const uint8_t* src_buffer,
                                                int32_t        src_size);

AARU_EXPORT int32_t AARU_CALL apple_rle_decode_buffer(uint8_t*       dst_buffer,
                                                      int32_t        dst_size,
                                                      const uint8_t* src_buffer,
                                                      int32_t        src_size);

AARU_EXPORT size_t AARU_CALL flac_decode_redbook_buffer(uint8_t*       dst_buffer,
                                                        size_t         dst_size,
                                                        const uint8_t* src_buffer,
                                                        size_t         src_size);

AARU_EXPORT size_t AARU_CALL flac_encode_redbook_buffer(uint8_t*       dst_buffer,
                                                        size_t         dst_size,
                                                        const uint8_t* src_buffer,
                                                        size_t         src_size,
                                                        uint32_t       blocksize,
                                                        int32_t        do_mid_side_stereo,
                                                        int32_t        loose_mid_side_stereo,
                                                        const char*    apodization,
                                                        uint32_t       qlp_coeff_precision,
                                                        int32_t        do_qlp_coeff_prec_search,
                                                        int32_t        do_exhaustive_model_search,
                                                        uint32_t       min_residual_partition_order,
                                                        uint32_t       max_residual_partition_order,
                                                        const char*    application_id,
                                                        uint32_t       application_id_len);

AARU_EXPORT int32_t AARU_CALL lzip_decode_buffer(uint8_t*       dst_buffer,
                                                 int32_t        dst_size,
                                                 const uint8_t* src_buffer,
                                                 int32_t        src_size);

AARU_EXPORT int32_t AARU_CALL lzip_encode_buffer(uint8_t*       dst_buffer,
                                                 int32_t        dst_size,
                                                 const uint8_t* src_buffer,
                                                 int32_t        src_size,
                                                 int32_t        dictionary_size,
                                                 int32_t        match_len_limit);

#endif // AARU_COMPRESSION_NATIVE_LIBRARY_H
