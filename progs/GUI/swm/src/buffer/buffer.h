#ifndef SWM_BUFFER_H
#define SWM_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct swm_buffer swm_buffer_t;

swm_buffer_t *swm_buffer_create(int handle, uint32_t width,
    uint32_t height, uint32_t stride, size_t size);
void swm_buffer_destroy(swm_buffer_t *buffer);
uint32_t swm_buffer_width(const swm_buffer_t *buffer);
uint32_t swm_buffer_height(const swm_buffer_t *buffer);
uint32_t swm_buffer_stride(const swm_buffer_t *buffer);
size_t swm_buffer_size(const swm_buffer_t *buffer);
const void *swm_buffer_pixels(const swm_buffer_t *buffer);

#endif
