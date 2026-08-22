#ifndef SWM_BACKEND_H
#define SWM_BACKEND_H

#include <srapi.h>

#include <stdint.h>

typedef struct swm_output swm_output_t;

int swm_output_create_default(swm_output_t **out);
void swm_output_destroy(swm_output_t *output);
uint32_t swm_output_width(const swm_output_t *output);
uint32_t swm_output_height(const swm_output_t *output);
srapi_device_t *swm_output_device(swm_output_t *output);
srapi_image_t *swm_output_backbuffer(swm_output_t *output);
int swm_output_present(swm_output_t *output, const struct srapi_region *region);

#endif
