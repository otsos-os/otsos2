#ifndef ONETOOL_LIBS_GUI_SWM_INTERACTION_H
#define ONETOOL_LIBS_GUI_SWM_INTERACTION_H

#include <swm/swm.h>

void swm_interaction_forward_input(swm_state_t *swm,
    const struct srapi_input_event *event);
void swm_surface_toggle_fullscreen(swm_state_t *swm, swm_surface_t *surface);

#endif
