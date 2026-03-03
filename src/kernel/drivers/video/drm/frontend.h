#ifndef DRM_FRONTEND_H
#define DRM_FRONTEND_H

#include <mlibc/mlibc.h>

int drm_frontend_is_available(void);
int drm_frontend_get_cols(void);
int drm_frontend_get_rows(void);

void drm_frontend_batch_begin(void);
void drm_frontend_batch_end(void);
void drm_frontend_flush(void);

void drm_frontend_clear(u32 color);
void drm_frontend_scroll_lines(int lines);
void drm_frontend_put_char_cell(int x, int y, char c, u32 fg_color);

#endif
