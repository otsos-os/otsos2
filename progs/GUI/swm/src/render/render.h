#ifndef ONETOOL_LIBS_GUI_SWM_RENDER_H
#define ONETOOL_LIBS_GUI_SWM_RENDER_H

#include <swm/swm.h>

void swm_render_composite(swm_state_t *swm, srapi_image_t *image,
    uint32_t bg_color);

#endif
