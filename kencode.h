// KenCode decompressor
// Reverse-engineered from Apple DiskCopy 6.3.3 hdi2 resource ID 128

#ifndef KENCODE_H
#define KENCODE_H

#include <stddef.h>
#include <stdint.h>

int kencode_decode_buffer(uint8_t *dst_buffer, size_t dst_size, const uint8_t *src_buffer, size_t src_size);

#endif  // KENCODE_H
