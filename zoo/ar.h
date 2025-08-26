/*$Source: /usr/home/dhesi/zoo/RCS/ar.h,v $*/
/*$Id: ar.h,v 1.17 91/07/09 01:39:50 dhesi Exp $*/
/***********************************************************
    ar.h

Adapted from "ar" archiver written by Haruhiko Okumura.
***********************************************************/
// Modified for in-memory decompression by Natalia Portillo, 2025

#include <stdint.h>
#include <stdio.h>

/* all the prototypes follow here for all files */

/* DECODE.C */
void decode_start();
int  decode(uint32_t count, uint8_t *buffer);

/* HUF.C */
void     output(uint32_t c, uint32_t p);
uint32_t decode_c(void);
uint32_t decode_p(void);
void     huf_decode_start(void);

/* IO.C */
void     fillbuf(int n);
uint32_t getbits(int n);
void     putbits(int n, uint32_t x);
void     init_getbits(void);
void     init_putbits(void);

/* MAKETBL.C */
void make_table(int nchar, uint8_t bitlen[], int tablebits, uint16_t table[]);

/* MAKETREE.C */
int make_tree(int nparm, uint16_t freqparm[], uint8_t lenparm[], uint16_t codeparm[]);

/* for lzh modules and also for ar.c to use in defining buffer size */
#define DICBIT 13 /* 12(-lh4-) or 13(-lh5-) */
#define DICSIZ ((unsigned)1 << DICBIT)
