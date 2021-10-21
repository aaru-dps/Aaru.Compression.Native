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

#endif // AARU_COMPRESSION_NATIVE__FLAC_H_
