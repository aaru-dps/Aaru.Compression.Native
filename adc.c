//
// Created by claunia on 17/10/21.
//

#include <stdint.h>
#include <string.h>

#include "library.h"
#include "adc.h"

FORCE_INLINE int GetChunkType(uint8_t byt)
{
    if(byt & 0x80) return ADC_PLAIN;
    if(byt & 0x40) return ADC_THREE_BYTE;
    return ADC_TWO_BYTE;
}

FORCE_INLINE int GetChunkSize(uint8_t byt)
{
    switch(GetChunkType(byt))
    {
        case ADC_PLAIN: return (byt & 0x7F) + 1;
        case ADC_TWO_BYTE: return ((byt & 0x3F) >> 2) + 3;
        case ADC_THREE_BYTE: return (byt & 0x3F) + 4;
        default: return -1;
    }
}

FORCE_INLINE int GetOffset(uint8_t chunk[])
{
    switch(GetChunkType(chunk[0]))
    {
        case ADC_PLAIN: return 0;
        case ADC_TWO_BYTE: return ((chunk[0] & 0x03) << 8) + chunk[1];
        case ADC_THREE_BYTE: return (chunk[1] << 8) + chunk[2];
        default: return -1;
    }
}

AARU_EXPORT int32_t AARU_CALL AARU_adc_decode_buffer(uint8_t*       dst_buffer,
                                                     int32_t        dst_size,
                                                     const uint8_t* src_buffer,
                                                     int32_t        src_size)
{
    int     inputPosition = 0;
    int     chunkSize;
    int     offset;
    int     chunkType;
    int     outPosition = 0;
    uint8_t temp[3];
    uint8_t readByte;
    uint8_t lastByte;
    int     i;

    while(inputPosition < src_size)
    {
        readByte = src_buffer[inputPosition++];

        chunkType = GetChunkType(readByte);

        switch(chunkType)
        {
            case ADC_PLAIN:
                chunkSize = GetChunkSize(readByte);
                if(outPosition + chunkSize > dst_size) goto finished;
                memcpy(dst_buffer + outPosition, src_buffer + inputPosition, chunkSize);
                outPosition += chunkSize;
                inputPosition += chunkSize;
                break;
            case ADC_TWO_BYTE:
                chunkSize = GetChunkSize(readByte);
                temp[0]   = readByte;
                temp[1]   = src_buffer[inputPosition++];
                offset    = GetOffset(temp);
                if(outPosition + chunkSize > dst_size) goto finished;
                if(offset == 0)
                {
                    lastByte = dst_buffer[outPosition - 1];
                    for(i = 0; i < chunkSize; i++)
                    {
                        dst_buffer[outPosition] = lastByte;
                        outPosition++;
                    }
                }
                else
                {
                    for(i = 0; i < chunkSize; i++)
                    {
                        dst_buffer[outPosition] = dst_buffer[outPosition - offset - 1];
                        outPosition++;
                    }
                }
                break;
            case ADC_THREE_BYTE:
                chunkSize = GetChunkSize(readByte);
                temp[0]   = readByte;
                temp[1]   = src_buffer[inputPosition++];
                temp[2]   = src_buffer[inputPosition++];
                offset    = GetOffset(temp);

                if(outPosition + chunkSize > dst_size) goto finished;
                if(offset == 0)
                {
                    lastByte = dst_buffer[outPosition - 1];
                    for(i = 0; i < chunkSize; i++)
                    {
                        dst_buffer[outPosition] = lastByte;
                        outPosition++;
                    }
                }
                else
                {
                    for(i = 0; i < chunkSize; i++)
                    {
                        dst_buffer[outPosition] = dst_buffer[outPosition - offset - 1];
                        outPosition++;
                    }
                }
                break;
        }
    }

finished:
    return outPosition;
}