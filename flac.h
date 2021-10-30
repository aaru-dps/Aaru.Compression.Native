//
// Created by claunia on 20/10/21.
//

#ifndef AARU_COMPRESSION_NATIVE__FLAC_H_
#define AARU_COMPRESSION_NATIVE__FLAC_H_

typedef struct
{
    const uint8_t* src_buffer;
    size_t         src_len;
    size_t         src_pos;
    uint8_t*       dst_buffer;
    size_t         dst_len;
    size_t         dst_pos;
    uint8_t        error;
} aaru_flac_ctx;

#endif // AARU_COMPRESSION_NATIVE__FLAC_H_
