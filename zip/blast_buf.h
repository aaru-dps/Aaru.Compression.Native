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

#ifndef AARU_COMPRESSION_NATIVE_ZIP_BLAST_BUF_H
#define AARU_COMPRESSION_NATIVE_ZIP_BLAST_BUF_H

#include <stddef.h>
#include <stdint.h>

/**
 * Decompress PKWare DCL Implode (blast) data from a buffer.
 *
 * This wraps the callback-based blast() API into a simple buffer interface.
 *
 * @param in_buf   Compressed input buffer
 * @param in_len   Length of compressed input
 * @param out_buf  Decompressed output buffer (must be pre-allocated)
 * @param out_len  On entry: size of output buffer. On exit: bytes actually written.
 * @return 0 on success, non-zero on error
 */
int zip_blast_decompress(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf, size_t *out_len);

#endif /* AARU_COMPRESSION_NATIVE_ZIP_BLAST_BUF_H */
