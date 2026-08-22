#ifndef ONETOOL_LIBS_GUI_SWM_RENDER_H
#define ONETOOL_LIBS_GUI_SWM_RENDER_H

#include <swm/swm.h>

void swm_damage_add(swm_state_t *swm, int32_t x, int32_t y,
    int32_t w, int32_t h);
void swm_damage_all(swm_state_t *swm);
void swm_render_composite(swm_state_t *swm, srapi_image_t *image,
    uint32_t bg_color, swm_rect_t *out_region, int *out_valid);

#endif
