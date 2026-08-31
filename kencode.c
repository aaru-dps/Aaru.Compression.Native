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

// KenCode decompressor
// Reverse-engineered from Apple DiskCopy 6.3.3 resource fork hdi2 ID 128
// Algorithm: LZSS with MSB-first bitstream and variable-length prefix codes
//
// Function map from PEF disassembly:
//   0x0000  plugin_entry     Component Manager dispatcher (selectors 0-3)
//   0x00CC  read_match_info  Reads one token (match or literal run) from bitstream
//   0x01AC  decompress       Main decompression loop
//   0x03E4  bs_init          Initialize bitstream reader
//   0x03F4  read_unary       Count consecutive 1-bits (MSB-first)
//   0x046C  read_bits        Read N bits from bitstream
//   0x0510  read_literals    Read N literal bytes from bitstream
//   0x05C0  decode_lit_count Decode literal run length
//   0x0750  decode_match_len Decode match length with prefix code
//   0x0A84  decode_offset    Decode match offset (multi-level)
//   0x0C2C  offset_dispatch  Select distance class based on output position

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "library.h"
#include "kencode.h"

// ============================================================================
// MSB-first bitstream reader
// ============================================================================

typedef struct
{
    const uint8_t *data;
    uint32_t       bit_pos;
    uint32_t       bit_end;
} kc_bs;

static inline void bs_init(kc_bs *bs, const uint8_t *data, size_t size)
{
    bs->data    = data;
    bs->bit_pos = 0;
    bs->bit_end = (uint32_t)(size * 8);
}

// PEF 0x046C: Read N bits MSB-first.
// The PEF reads a 32-bit big-endian word at (bit_pos / 8), shifts right
// by (32 - bit_within_byte - n), and masks to n bits.
static inline uint32_t bs_read(kc_bs *bs, int n)
{
    if(n <= 0 || bs->bit_pos + (uint32_t)n > bs->bit_end) return 0;

    uint32_t byte_off = bs->bit_pos >> 3;
    int      bit_off  = bs->bit_pos & 7;
    int      shift    = 32 - bit_off - n;

    // Read a 32-bit big-endian word starting at byte_off
    // Need to handle reads near end of buffer
    uint32_t word     = 0;
    size_t   max_byte = (bs->bit_end + 7) / 8;
    for(int i = 0; i < 4; i++)
    {
        word <<= 8;
        if(byte_off + i < max_byte) word |= bs->data[byte_off + i];
    }

    uint32_t mask = ((uint32_t)1 << n) - 1;
    bs->bit_pos += n;

    if(shift >= 0) return (word >> shift) & mask;

    // shift < 0: need bits from next word (rare: n > 24 or misaligned)
    word <<= (-shift);
    if(byte_off + 4 < max_byte) word |= (bs->data[byte_off + 4] >> (8 + shift));
    return word & mask;
}

// PEF 0x03F4: Count consecutive 1-bits, stopping at first 0-bit or max_count.
static inline uint32_t bs_read_unary(kc_bs *bs, int max_count)
{
    uint32_t count = 0;
    while((int)count < max_count && bs->bit_pos < bs->bit_end)
    {
        uint32_t byte_off = bs->bit_pos >> 3;
        int      bit_off  = bs->bit_pos & 7;
        bs->bit_pos++;
        if(!(bs->data[byte_off] & (0x80 >> bit_off))) return count;  // hit 0
        count++;
    }
    return count;
}

// PEF 0x0510: Read N literal bytes from bitstream. Each byte is 8 bits
// at the current (possibly unaligned) bit position.
static void bs_read_bytes(kc_bs *bs, uint8_t *dst, int count)
{
    uint32_t byte_off = bs->bit_pos >> 3;
    int      bit_off  = bs->bit_pos & 7;
    int      rshift   = 8 - bit_off;

    for(int i = 0; i < count; i++)
    {
        uint16_t w = ((uint16_t)bs->data[byte_off] << 8) | bs->data[byte_off + 1];
        dst[i]     = (uint8_t)(w >> rshift);
        byte_off++;
    }
    bs->bit_pos += count * 8;
}

// ============================================================================
// VLC decoders
// ============================================================================

// PEF 0x05C0: Decode literal run length (1..63)
static int decode_lit_count(kc_bs *bs)
{
    // "0" → 1
    if(bs_read(bs, 1) == 0) return 1;

    // "1" + 2 bits
    uint32_t v = bs_read(bs, 2);
    if(v == 0) return 2;
    if(v == 1) return 3;
    if(v == 2) return (int)bs_read(bs, 2) + 4;  // 4-7

    // v==3: 4 more bits
    uint32_t e = bs_read(bs, 4);
    if(e <= 7) return (int)e + 8;                           // 8-15
    if(e <= 11) return (int)(e * 4 + bs_read(bs, 2)) - 16;  // 16-31
    return (int)(e * 8 + bs_read(bs, 3)) - 64;              // 32-63
}

// PEF 0x0750: Decode match length raw value (0..2042+)
// Value 0 with had_literals=1 → literal run marker
// Otherwise: actual match length = raw + 2 (or +3 if previous was literals)
static int decode_match_len(kc_bs *bs)
{
    uint32_t pfx = bs_read_unary(bs, 10);

    switch(pfx)
    {
        case 0:
            return (int)bs_read(bs, 1);  // 0-1
        case 1:
        {
            uint32_t b = bs_read(bs, 1);
            if(b == 0) return 2;
            return (int)bs_read(bs, 1) + 3;  // 3-4
        }
        case 2:
        {
            uint32_t b = bs_read(bs, 1);
            if(b == 0) return (int)bs_read(bs, 1) + 5;  // 5-6
            return (int)bs_read(bs, 2) + 7;             // 7-10
        }
        case 3:
            return (int)bs_read(bs, 3) + 11;  // 11-18
        case 4:
            return (int)bs_read(bs, 3) + 19;  // 19-26
        case 5:
            return (int)bs_read(bs, 5) + 27;  // 27-58
        case 6:
            return (int)bs_read(bs, 6) + 59;  // 59-122
        case 7:
            return (int)bs_read(bs, 7) + 123;  // 123-250
        case 8:
            return (int)bs_read(bs, 8) + 251;  // 251-506
        case 9:
            return (int)bs_read(bs, 9) + 507;  // 507-1018
        default:
            return (int)bs_read(bs, 10) + 1019;  // 1019+
    }
}

// PEF 0x0A84: Decode match offset given distance class (dist_bits)
static int decode_offset(kc_bs *bs, int dist_bits, int hash_size)
{
    int pw = 1 << dist_bits;

    // Prefix "0" + dist_bits → short offset
    if(bs_read(bs, 1) == 0) return (int)bs_read(bs, dist_bits) + 1;

    // Prefix "10" + (dist_bits+2) bits → medium offset
    if(bs_read(bs, 1) == 0) return pw + 1 + (int)bs_read(bs, dist_bits + 2);

    // Prefix "11" → long offset, escalating levels
    int pw5 = pw * 5;

    if(hash_size <= pw5 + 2) return pw5 + 1 + (int)bs_read(bs, 1);

    if(hash_size <= pw5 + 4) return pw5 + 1 + (int)bs_read(bs, 2);

    // Escalating loop
    int max_level = dist_bits + 4;
    int scale     = 4;
    int level     = 3;
    int threshold = pw5 + 4;

    if(max_level >= 3)
    {
        while(level <= max_level)
        {
            threshold += scale;
            // 68K helper at 0x0ABA: clamp ONLY the exact value 0x680 to 0x66C
            int clamped = (threshold == 0x680) ? 0x66C : threshold;
            if(hash_size <= clamped || level == max_level) break;
            scale <<= 1;
            level++;
        }
    }

    return pw5 + 1 + (int)bs_read(bs, level);
}

// PEF 0x0C2C / 68K 0x0D48: Select distance class based on output position and call decode_offset
// out_pos determines the distance class, hash_size (from state init) caps it for large positions
static int offset_dispatch(kc_bs *bs, int out_pos, int hash_size)
{
    int dist_bits;

    if(out_pos <= 0xa)
        dist_bits = 0;
    else if(out_pos <= 0x14)
        dist_bits = 1;
    else if(out_pos <= 0x28)
        dist_bits = 2;
    else if(out_pos <= 0x50)
        dist_bits = 3;
    else if(out_pos <= 0xa0)
        dist_bits = 4;
    else if(out_pos <= 0x2a0)
        dist_bits = 5;
    else if(out_pos <= 0x3e8)
        dist_bits = 6;
    else if(out_pos <= 0xa80 || hash_size <= 0x800)
        dist_bits = 7;
    else if(out_pos <= 0x1500 || hash_size <= 0x1000)
        dist_bits = 8;
    else if(out_pos <= 0x2a00 || hash_size <= 0x2000)
        dist_bits = 9;
    else if(out_pos <= 0x5400 || hash_size <= 0x4000)
        dist_bits = 10;
    else if(out_pos <= 0xa800 || hash_size <= 0x8000)
        dist_bits = 11;
    else if(out_pos <= 0x11170 || hash_size <= 0x10000)
        dist_bits = 12;
    else if(out_pos <= 0x2a000 || hash_size <= 0x20000)
        dist_bits = 13;
    else
        dist_bits = 14;

    // 68K passes (out_pos, dist_bits, bitstream) to decode_offset
    // but decode_offset internally uses out_pos for hash_size comparisons too
    return decode_offset(bs, dist_bits, out_pos);
}

// ============================================================================
// Main decompressor — PEF 0x01AC
// ============================================================================

AARU_EXPORT int32_t AARU_CALL AARU_kencode_decode_buffer(const uint8_t *src_buffer, size_t src_size,
                                                         uint8_t *dst_buffer, size_t *dst_size)
{
    if(!dst_buffer || !dst_size || !src_buffer || src_size == 0) return -1;

    size_t max_out = *dst_size;
    kc_bs  bs;
    bs_init(&bs, src_buffer, src_size);

    size_t  out_pos      = 0;
    uint8_t had_literals = 1;  // PEF 0x01C0-0x01C4

    // Plugin init: hash_size from clamp_hash_size(0x2800) → 0x2800 (in range [0x800, 0x30000])
    int hash_size = 0x2800;

    while(out_pos < max_out)
    {
        // PEF 0x00CC: read_match_info
        int raw = decode_match_len(&bs);

        if(raw <= 0 && had_literals)
        {
            // Literal run
            int lit_count = decode_lit_count(&bs);
            if(out_pos + lit_count > max_out) return -4;
            bs_read_bytes(&bs, dst_buffer + out_pos, lit_count);
            out_pos += lit_count;

            // 68K 0x03C4-0x03D2: had_literals = (lit_count >= 63) ? 1 : 0
            // PPC 0x0180-0x0194: equivalent via subfc/adde/subfe/clrlwi
            had_literals = (lit_count >= 63) ? 1 : 0;
        }
        else
        {
            // Match reference
            int match_len;
            if(had_literals == 0)
                match_len = raw + 3;  // PEF 0x0130: +3 when coming after non-literal
            else
                match_len = raw + 2;  // PEF 0x011C: +2 when had_literals is set

            had_literals = 1;  // PEF 0x0140

            int offset = offset_dispatch(&bs, (int)out_pos, hash_size);

            if(out_pos + match_len > max_out) return -4;
            if(offset > (int)out_pos) return -4;

            // Byte-by-byte copy (may overlap)
            for(int i = 0; i < match_len; i++) dst_buffer[out_pos + i] = dst_buffer[out_pos - offset + i];
            out_pos += match_len;
        }
    }

    *dst_size = out_pos;
    return 0;
}
