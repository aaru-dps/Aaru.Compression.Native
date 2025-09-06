
/***********************************************************************
  Modified to work with memory buffers instead of files by
  Copyright (C) 2005 Natalia Portillo
************************************************************************/

#include "internal.h"
#include "asc.h"
#include "hsc.h"
#include "../library.h"
#include <string.h>

int ha_algorithm_decompress(ha_algorithm_t       algorithm,
                            const unsigned char *in_buf,
                            size_t               in_len,
                            unsigned char *      out_buf,
                            size_t *             out_len)
{
    if(!in_buf || !out_buf || !out_len || in_len == 0) { return -1; }

    decompress_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* Initialize input buffer */
    ctx.input.data  = in_buf;
    ctx.input.size  = in_len;
    ctx.input.pos   = 0;
    ctx.input.error = 0;

    /* Initialize output buffer */
    ctx.output.data     = out_buf;
    ctx.output.size     = 0;
    ctx.output.pos      = 0;
    ctx.output.max_size = *out_len;
    ctx.output.error    = 0;

    ctx.algorithm = algorithm;

    int result;
    switch(algorithm)
    {
        case HA_ALGORITHM_ASC:
            result = asc_unpack(&ctx);
            break;
        case HA_ALGORITHM_HSC:
            result = hsc_unpack(&ctx);
            break;
        default:
            return -1;
    }

    *out_len = ctx.output.size;
    return result;
}

AARU_EXPORT int AARU_CALL ha_asc_decompress(const unsigned char *in_buf,
                                            size_t               in_len,
                                            unsigned char *      out_buf,
                                            size_t *             out_len)
{
    return ha_algorithm_decompress(HA_ALGORITHM_ASC, in_buf, in_len, out_buf, out_len);
}

AARU_EXPORT int AARU_CALL ha_hsc_decompress(const unsigned char *in_buf,
                                            size_t               in_len,
                                            unsigned char *      out_buf,
                                            size_t *             out_len)
{
    return ha_algorithm_decompress(HA_ALGORITHM_HSC, in_buf, in_len, out_buf, out_len);
}
