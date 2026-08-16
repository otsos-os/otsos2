#ifndef ONETOOL_LIBS_GUI_SPROT_SRAPI_BRIDGE_H
#define ONETOOL_LIBS_GUI_SPROT_SRAPI_BRIDGE_H

#include <sprot/client.h>
#include <srapi.h>

sprot_surface_t *sprot_create_surface_for_image(
    sprot_connection_t *conn,
    srapi_image_t *image,
    const char *title
);

int sprot_present_image(
    sprot_surface_t *surface,
    srapi_image_t *image
);

#endif
