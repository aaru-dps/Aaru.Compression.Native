
/***********************************************************************
  Modified to work with memory buffers instead of files by
  Copyright (C) 2005 Natalia Portillo
************************************************************************/

#ifndef INTERNAL_H
#define INTERNAL_H

#include <stdint.h>
#include <stddef.h>

/* Type definitions matching original HA code */
typedef int16_t  S16B;
typedef uint16_t U16B;
typedef int32_t  S32B;
typedef uint32_t U32B;

/* Memory buffer management */
typedef struct
{
    const unsigned char *data;
    size_t               size;
    size_t               pos;
    int                  error;
} input_buffer_t;

typedef struct
{
    unsigned char *data;
    size_t         size;
    size_t         pos;
    size_t         max_size;
    int            error;
} output_buffer_t;

/* Decompression context */
typedef struct
{
    input_buffer_t  input;
    output_buffer_t output;
    int             algorithm;
} decompress_context_t;

#endif /* INTERNAL_H */
