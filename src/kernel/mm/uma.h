/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#ifndef UMA_H
#define UMA_H

#include <mlibc/mlibc.h>

#define UMA_ZONE_MAX       64
#define UMA_BUCKET_SIZE    32
#define UMA_ALIGN_CACHE    64

typedef struct uma_zone *uma_zone_t;

#define M_WAITOK  0x0000
#define M_NOWAIT  0x0001
#define M_ZERO    0x0100

uma_zone_t uma_zcreate(const char *name, unsigned long size,
                       unsigned long align, u32 flags);
void      uma_zdestroy(uma_zone_t zone);
void     *uma_zalloc(uma_zone_t zone, u32 flags);
void      uma_zfree(uma_zone_t zone, void *item);

void uma_init(void);
void uma_dump(void);

uma_zone_t uma_zfind(const char *name);

#endif
