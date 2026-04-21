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

#ifndef AARU_COMPRESSION_NATIVE_ZIP_ZIP_H
#define AARU_COMPRESSION_NATIVE_ZIP_ZIP_H

#include <stddef.h>
#include <stdint.h>

#include "blast_buf.h"
#include "deflate64.h"
#include "implode.h"
#include "reduce.h"
#include "shrink.h"

/**
 * Decompress ZIP PPMd (method 98, variant I) data.
 *
 * @param dst_buffer     Output buffer
 * @param dst_size       Size of output buffer / bytes to decompress
 * @param src_buffer     Compressed input buffer
 * @param src_size       Size of compressed input
 * @param max_order      PPMd model order (1-16)
 * @param sub_alloc_size Sub-allocator memory size in bytes
 * @param restoration    Model restoration method (0=restart, 1=cutoff, 2=freeze)
 * @return 0 on success, non-zero on error
 */
int zip_ppmd_decompress(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size, int max_order,
                        int sub_alloc_size, int restoration);

/**
 * Decompress WinZip WavPack (ZIP method 97) data.
 *
 * @param dst_buffer      Output buffer for raw audio samples
 * @param dst_size        On entry: size of output buffer. On exit: bytes written.
 * @param src_buffer      Compressed WavPack data
 * @param src_size        Size of compressed input
 * @param num_samples     Number of audio samples to decode
 * @param bits_per_sample Bits per sample (8, 16, 24, 32)
 * @param num_channels    Number of audio channels
 * @return 0 on success, non-zero on error
 */
int zip_wavpack_decompress(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size,
                           uint32_t num_samples, int bits_per_sample, int num_channels);

/**
 * Decompress WinZip JPEG (ZIP method 96) data.
 *
 * @param dst_buffer  Output buffer for reconstructed JPEG data
 * @param dst_size    On entry: size of output buffer. On exit: bytes written.
 * @param src_buffer  Compressed WinZipJPEG data
 * @param src_size    Size of compressed input
 * @return 0 on success, non-zero on error
 */
int zip_winzipjpeg_decompress(uint8_t *dst_buffer, size_t *dst_size, const uint8_t *src_buffer, size_t src_size);

#endif /* AARU_COMPRESSION_NATIVE_ZIP_ZIP_H */
