//
// Created by claunia on 17/10/21.
//

#ifndef AARU_COMPRESSION_NATIVE__ADC_H_
#define AARU_COMPRESSION_NATIVE__ADC_H_

#define ADC_PLAIN 1
#define ADC_TWO_BYTE 2
#define ADC_THREE_BYTE 3

AARU_EXPORT int32_t AARU_CALL adc_decode_buffer(uint8_t*       dst_buffer,
                                                int32_t        dst_size,
                                                const uint8_t* src_buffer,
                                                int32_t        src_size);

#endif // AARU_COMPRESSION_NATIVE__ADC_H_
