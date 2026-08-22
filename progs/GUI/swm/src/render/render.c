#include "render.h"

#include "../buffer/buffer.h"
#include "../surface/surface.h"

#include <string.h>

typedef struct render_target {
    uint32_t *pixels;
    int32_t pitch_px;
    int32_t w, h;
    int32_t cx0, cy0, cx1, cy1;
} render_target_t;

static void fill_rect(const render_target_t *t,
                      int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    int32_t x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < t->cx0) x0 = t->cx0;
    if (y0 < t->cy0) y0 = t->cy0;
    if (x1 > t->cx1) x1 = t->cx1;
    if (y1 > t->cy1) y1 = t->cy1;
    if (x0 >= x1 || y0 >= y1) return;
    for (int32_t row = y0; row < y1; row++) {
        uint32_t *dr = t->pixels + row * t->pitch_px;
        for (int32_t col = x0; col < x1; col++) dr[col] = color;
    }
}

static const uint8_t SWM_FONT_5x7[95][7];

static void draw_glyph(const render_target_t *t,
                       int32_t x, int32_t y, char c, uint32_t color) {
    int idx = (c >= 32 && c <= 126) ? (c - 32) : ('?' - 32);
    const uint8_t *g = SWM_FONT_5x7[idx];
    for (int gy = 0; gy < 7; gy++) {
        uint8_t row = g[gy];
        for (int gx = 0; gx < 5; gx++) {
            if (row & (1 << (4 - gx))) {
                int32_t px = x + gx, py = y + gy;
                if (px >= t->cx0 && px < t->cx1 &&
                    py >= t->cy0 && py < t->cy1) {
                    t->pixels[py * t->pitch_px + px] = color;
                }
            }
        }
    }
}

static int32_t draw_text(const render_target_t *t,
                         int32_t x, int32_t y, const char *s, uint32_t color, int32_t max_w) {
    int32_t cur = x;
    for (; *s; s++) {
        if (cur + 5 > x + max_w) break;
        draw_glyph(t, cur, y, *s, color);
        cur += 6;
    }
    return cur;
}

static void draw_cursor(const render_target_t *t,
                        int32_t cx, int32_t cy, uint32_t cursor_type) {
    static const uint8_t arrow[16][12] = {
        {1,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0,0},
        {1,2,2,2,2,2,1,1,1,1,0,0},
        {1,2,2,1,2,2,1,0,0,0,0,0},
        {1,2,1,0,1,2,2,1,0,0,0,0},
        {1,1,0,0,1,2,2,1,0,0,0,0},
        {0,0,0,0,0,1,2,2,1,0,0,0},
        {0,0,0,0,0,1,2,2,1,0,0,0},
        {0,0,0,0,0,0,1,1,0,0,0,0},
    };
    static const uint8_t ibeam[16][12] = {
        {0,0,0,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,0,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,0,0,0,0},
    };
    static const uint8_t hand[16][12] = {
        {0,0,0,0,1,1,0,0,0,0,0,0},
        {0,0,0,1,2,2,1,0,0,0,0,0},
        {0,0,0,1,2,2,1,0,0,0,0,0},
        {0,0,0,1,2,2,1,0,0,0,0,0},
        {0,0,0,1,2,2,1,1,1,0,0,0},
        {0,0,1,1,2,2,2,2,2,1,0,0},
        {0,1,2,2,2,2,2,2,2,2,1,0},
        {0,1,2,2,2,2,2,2,2,2,1,0},
        {1,2,2,2,2,2,2,2,2,2,1,0},
        {1,2,2,2,2,2,2,2,2,2,1,0},
        {1,2,2,2,2,2,2,2,2,2,1,0},
        {0,1,2,2,2,2,2,2,2,1,0,0},
        {0,0,1,2,2,2,2,2,2,1,0,0},
        {0,0,0,1,2,2,2,2,1,0,0,0},
        {0,0,0,0,1,2,2,1,0,0,0,0},
        {0,0,0,0,0,1,1,0,0,0,0,0},
    };
    const uint8_t (*sprite)[12] = arrow;
    int off_x = 0, off_y = 0;
    if (cursor_type == SPROT_CURSOR_IBEAM) { sprite = ibeam; off_x = -5; off_y = -8; }
    else if (cursor_type == SPROT_CURSOR_HAND) { sprite = hand; off_x = -4; off_y = 0; }

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 12; x++) {
            uint8_t v = sprite[y][x];
            if (v == 0) continue;
            int32_t px = cx + x + off_x, py = cy + y + off_y;
            if (px < t->cx0 || px >= t->cx1 ||
                py < t->cy0 || py >= t->cy1) continue;
            t->pixels[py * t->pitch_px + px] = (v == 1) ? 0xFF000000u : 0xFFFFFFFFu;
        }
    }
}

static void draw_cursor_image(swm_state_t *swm, const render_target_t *t,
                              int32_t cx, int32_t cy) {
    swm_buffer_t *buffer = swm->cursor_buffer;
    if (buffer == NULL || !swm->cursor_visible) return;
    const uint32_t *src = swm_buffer_pixels(buffer);
    int32_t src_w = (int32_t)swm_buffer_width(buffer);
    int32_t src_h = (int32_t)swm_buffer_height(buffer);
    int32_t src_pitch_px = (int32_t)(swm_buffer_stride(buffer) / 4u);
    int32_t x0 = cx - swm->cursor_hotspot_x;
    int32_t y0 = cy - swm->cursor_hotspot_y;

    if (src != NULL) {
        for (int32_t y = 0; y < src_h; y++) {
            int32_t dy = y0 + y;
            if (dy < t->cy0 || dy >= t->cy1) continue;
            const uint32_t *sr = src + y * src_pitch_px;
            uint32_t *dr = t->pixels + dy * t->pitch_px;
            for (int32_t x = 0; x < src_w; x++) {
                int32_t dx = x0 + x;
                if (dx < t->cx0 || dx >= t->cx1) continue;
                uint32_t sp = sr[x];
                uint32_t a = sp >> 24;
                if (a == 0) continue;
                if (a == 255) {
                    dr[dx] = sp;
                    continue;
                }
                uint32_t dp = dr[dx];
                uint32_t rb = (sp & 0x00FF00FFu) + (((dp & 0x00FF00FFu) * (255u - a)) >> 8);
                uint32_t g = (sp & 0x0000FF00u) + (((dp & 0x0000FF00u) * (255u - a)) >> 8);
                dr[dx] = 0xFF000000u | (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
            }
        }
    }
}

static uint32_t blend_premul_pixel(uint32_t dst, uint32_t src) {
    uint32_t a = src >> 24;
    if (a == 0) return dst;
    if (a == 255) return src;
    uint32_t inv = 255u - a;
    uint32_t rb = (src & 0x00FF00FFu) + (((dst & 0x00FF00FFu) * inv) >> 8);
    uint32_t g = (src & 0x0000FF00u) + (((dst & 0x0000FF00u) * inv) >> 8);
    return 0xFF000000u | (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
}

static void draw_titlebar_chrome(swm_state_t *swm, const render_target_t *t,
                                 swm_surface_t *s, int is_focused) {
    uint32_t bar_color    = is_focused ? 0xFF3A6CB0u : 0xFF333742u;
    uint32_t border_color = is_focused ? 0xFF5AA0F0u : 0xFF4A4F5Bu;
    uint32_t text_color   = is_focused ? 0xFFFFFFFFu : 0xFFB5BAC4u;
    int32_t outer_x, outer_y, outer_w, outer_h;
    swm_surface_outer_rect(swm, s, &outer_x, &outer_y, &outer_w, &outer_h);

    fill_rect(t, outer_x, outer_y, outer_w, SWM_BORDER, border_color);
    fill_rect(t, outer_x, outer_y + outer_h - SWM_BORDER, outer_w, SWM_BORDER, border_color);
    fill_rect(t, outer_x, outer_y, SWM_BORDER, outer_h, border_color);
    fill_rect(t, outer_x + outer_w - SWM_BORDER, outer_y, SWM_BORDER, outer_h, border_color);
    fill_rect(t, outer_x + SWM_BORDER, outer_y + SWM_BORDER,
              outer_w - 2 * SWM_BORDER, SWM_TITLEBAR_H, bar_color);

    int32_t bmin, bmax, bclose, by;
    swm_surface_titlebar_button_rects(swm, s, &bmin, &bmax, &bclose, &by);
    fill_rect(t, bmin,   by, SWM_BTN_SIZE, SWM_BTN_SIZE, 0xFFD0B040u);
    fill_rect(t, bmax,   by, SWM_BTN_SIZE, SWM_BTN_SIZE, 0xFF40C060u);
    fill_rect(t, bclose, by, SWM_BTN_SIZE, SWM_BTN_SIZE, 0xFFE05050u);

    int32_t title_x = outer_x + SWM_BORDER + 6;
    int32_t title_y = outer_y + SWM_BORDER + (SWM_TITLEBAR_H - 7) / 2;
    int32_t title_max = bmin - title_x - 6;
    if (title_max > 0) {
        draw_text(t, title_x, title_y, s->title, text_color, title_max);
    }
}

void swm_damage_add(swm_state_t *swm, int32_t x, int32_t y,
                    int32_t w, int32_t h) {
    int32_t x2, y2;

    if (swm == NULL || swm->damage_full || w <= 0 || h <= 0) return;
    x2 = x + w;
    y2 = y + h;
    if (!swm->damage_valid) {
        swm->damage.x = x;
        swm->damage.y = y;
        swm->damage.w = w;
        swm->damage.h = h;
        swm->damage_valid = 1;
        return;
    }
    if (x < swm->damage.x) {
        swm->damage.w += swm->damage.x - x;
        swm->damage.x = x;
    }
    if (y < swm->damage.y) {
        swm->damage.h += swm->damage.y - y;
        swm->damage.y = y;
    }
    if (x2 > swm->damage.x + swm->damage.w) swm->damage.w = x2 - swm->damage.x;
    if (y2 > swm->damage.y + swm->damage.h) swm->damage.h = y2 - swm->damage.y;
}

void swm_damage_all(swm_state_t *swm) {
    if (swm == NULL) return;
    swm->damage_full = 1;
    swm->damage_valid = 1;
}

static uint32_t title_hash(const char *s) {
    uint32_t hash = 2166136261u;
    for (; *s; s++) {
        hash ^= (uint8_t)*s;
        hash *= 16777619u;
    }
    return hash;
}

static void damage_record(swm_state_t *swm, const swm_paint_record_t *rec) {
    if (rec->painted) {
        swm_damage_add(swm, rec->outer.x, rec->outer.y,
                       rec->outer.w, rec->outer.h);
    }
}

static void collect_damage(swm_state_t *swm, swm_surface_t **list, int n,
                           swm_surface_t *focused, swm_paint_record_t *cur) {
    int slot, i;

    memset(cur, 0, sizeof(*cur) * SWM_MAX_SURFACES);

    for (i = 0; i < n; i++) {
        swm_surface_t *s = list[i];
        swm_paint_record_t *rec;
        int32_t ox, oy, ow, oh;

        slot = (int)(s - swm->surfaces);
        if (slot < 0 || slot >= SWM_MAX_SURFACES) continue;
        rec = &cur[slot];

        swm_surface_outer_rect(swm, s, &ox, &oy, &ow, &oh);
        rec->outer.x = ox;
        rec->outer.y = oy;
        rec->outer.w = ow;
        rec->outer.h = oh;
        rec->has_chrome = (!s->fullscreen &&
                           !swm_surface_role_is_child(s->role) &&
                           !swm_surface_role_is_panel(s->role));
        rec->focused = (s == focused);
        rec->pixels = (s->buffer != NULL) ? swm_buffer_pixels(s->buffer) : NULL;
        rec->content_serial = s->content_serial;
        rec->title_hash = title_hash(s->title);
        rec->painted = 1;
    }

    for (slot = 0; slot < SWM_MAX_SURFACES; slot++) {
        swm_paint_record_t *old = &swm->paint[slot];
        swm_paint_record_t *now = &cur[slot];
        swm_surface_t *s = &swm->surfaces[slot];

        if (!now->painted) {
            damage_record(swm, old);
            continue;
        }
        if (!old->painted) {
            damage_record(swm, now);
            continue;
        }
        if (old->outer.x != now->outer.x || old->outer.y != now->outer.y ||
            old->outer.w != now->outer.w || old->outer.h != now->outer.h ||
            old->focused != now->focused ||
            old->has_chrome != now->has_chrome ||
            old->title_hash != now->title_hash ||
            old->pixels != now->pixels) {
            damage_record(swm, old);
            damage_record(swm, now);
            continue;
        }

        if (old->content_serial != now->content_serial) {
            int32_t ex, ey, ew, eh;

            swm_surface_effective_rect(swm, s, &ex, &ey, &ew, &eh);
            if (s->damage_valid) {
                int32_t dx = ex + s->damage.x;
                int32_t dy = ey + s->damage.y;
                int32_t dw = s->damage.w;
                int32_t dh = s->damage.h;
                if (dx < ex) { dw -= ex - dx; dx = ex; }
                if (dy < ey) { dh -= ey - dy; dy = ey; }
                if (dx + dw > ex + ew) dw = ex + ew - dx;
                if (dy + dh > ey + eh) dh = ey + eh - dy;
                swm_damage_add(swm, dx, dy, dw, dh);
            } else {
                swm_damage_add(swm, ex, ey, ew, eh);
            }
        }
    }

}

static void cursor_rect(swm_state_t *swm, swm_rect_t *out) {
    int32_t w = 12, h = 16, hx = 0, hy = 8;

    if (swm->cursor_buffer != NULL) {
        w = (int32_t)swm_buffer_width(swm->cursor_buffer);
        h = (int32_t)swm_buffer_height(swm->cursor_buffer);
        hx = swm->cursor_hotspot_x;
        hy = swm->cursor_hotspot_y;
    }
    out->x = swm->mouse_x - hx - 8;
    out->y = swm->mouse_y - hy - 8;
    out->w = w + 16;
    out->h = h + 16;
}

static void collect_cursor_damage(swm_state_t *swm, const swm_rect_t *now) {
    const void *pixels;
    int visible, moved;

    pixels = (swm->cursor_buffer != NULL) ?
        swm_buffer_pixels(swm->cursor_buffer) : NULL;
    visible = (swm->cursor_buffer != NULL && swm->cursor_visible);

    if (!swm->cursor_prev_valid) {
        swm_damage_add(swm, now->x, now->y, now->w, now->h);
        return;
    }
    moved = (swm->cursor_prev.x != now->x || swm->cursor_prev.y != now->y ||
             swm->cursor_prev.w != now->w || swm->cursor_prev.h != now->h);
    if (!moved && swm->cursor_prev_pixels == pixels &&
        swm->cursor_prev_type == swm->current_cursor &&
        swm->cursor_prev_visible == visible) {
        return;
    }
    swm_damage_add(swm, swm->cursor_prev.x, swm->cursor_prev.y,
                   swm->cursor_prev.w, swm->cursor_prev.h);
    swm_damage_add(swm, now->x, now->y, now->w, now->h);
}

static void cursor_commit(swm_state_t *swm, const swm_rect_t *now) {
    swm->cursor_prev = *now;
    swm->cursor_prev_pixels = (swm->cursor_buffer != NULL) ?
        swm_buffer_pixels(swm->cursor_buffer) : NULL;
    swm->cursor_prev_type = swm->current_cursor;
    swm->cursor_prev_visible = (swm->cursor_buffer != NULL &&
        swm->cursor_visible);
    swm->cursor_prev_valid = 1;
}

void swm_render_composite(swm_state_t *swm, srapi_image_t *image,
                          uint32_t bg_color, swm_rect_t *out_region,
                          int *out_valid) {
    swm_paint_record_t cur[SWM_MAX_SURFACES];
    render_target_t target;
    swm_rect_t crect;
    uint32_t *dst = srapiImagePixels(image);
    int32_t dst_w = (int32_t)srapiImageWidth(image);
    int32_t dst_h = (int32_t)srapiImageHeight(image);
    int32_t dst_pitch_px = (int32_t)(srapiImagePitch(image) / 4u);

    if (out_valid != NULL) *out_valid = 0;
    if (dst == NULL) return;

    swm_surface_t *list[SWM_MAX_SURFACES];
    int n = swm_surface_collect_z_asc(swm, list, SWM_MAX_SURFACES);
    swm_surface_t *focused = NULL;
    for (int i = n - 1; i >= 0; i--) {
        if (swm_surface_role_is_window(list[i]->role)) {
            focused = list[i];
            break;
        }
    }

    collect_damage(swm, list, n, focused, cur);
    cursor_rect(swm, &crect);
    collect_cursor_damage(swm, &crect);

    target.pixels = dst;
    target.pitch_px = dst_pitch_px;
    target.w = dst_w;
    target.h = dst_h;
    if (swm->damage_full || !swm->damage_valid) {
        target.cx0 = 0;
        target.cy0 = 0;
        target.cx1 = dst_w;
        target.cy1 = dst_h;
    } else {
        target.cx0 = swm->damage.x < 0 ? 0 : swm->damage.x;
        target.cy0 = swm->damage.y < 0 ? 0 : swm->damage.y;
        target.cx1 = swm->damage.x + swm->damage.w;
        target.cy1 = swm->damage.y + swm->damage.h;
        if (target.cx1 > dst_w) target.cx1 = dst_w;
        if (target.cy1 > dst_h) target.cy1 = dst_h;
    }

    if (target.cx0 >= target.cx1 || target.cy0 >= target.cy1) {
        swm->damage_valid = 0;
        swm->damage_full = 0;
        memcpy(swm->paint, cur, sizeof(cur));
        cursor_commit(swm, &crect);
        return;
    }

    fill_rect(&target, target.cx0, target.cy0,
              target.cx1 - target.cx0, target.cy1 - target.cy0, bg_color);

    for (int i = 0; i < n; i++) {
        swm_surface_t *s = list[i];
		if (!s->fullscreen &&
		    !swm_surface_role_is_child(s->role) &&
		    !swm_surface_role_is_panel(s->role)) {
            draw_titlebar_chrome(swm, &target, s, s == focused);
        }

        if (s->buffer == NULL) continue;
        int32_t ex, ey, ew, eh;
        swm_surface_effective_rect(swm, s, &ex, &ey, &ew, &eh);
        int32_t src_pitch_px = (int32_t)(swm_buffer_stride(s->buffer) / 4u);
        const uint32_t *src = (const uint32_t *)swm_buffer_pixels(s->buffer);
        int32_t src_w = (int32_t)s->width;
        int32_t src_h = (int32_t)s->height;
        if (src == NULL) continue;
        (void)ew; (void)eh;
        int32_t sx0 = 0, sy0 = 0;
        int32_t w = src_w, h = src_h;
        int32_t dx = ex, dy = ey;

        if (dx < target.cx0) { sx0 = target.cx0 - dx; w -= sx0; dx = target.cx0; }
        if (dy < target.cy0) { sy0 = target.cy0 - dy; h -= sy0; dy = target.cy0; }
        if (dx + w > target.cx1) w = target.cx1 - dx;
        if (dy + h > target.cy1) h = target.cy1 - dy;
        if (w > 0 && h > 0 && sx0 < src_w && sy0 < src_h) {
            int blend_child = swm_surface_role_is_child(s->role) && s->buffer_has_alpha;
            for (int32_t row = 0; row < h; row++) {
                const uint32_t *sr = src + (sy0 + row) * src_pitch_px + sx0;
                uint32_t *dr = dst + (dy + row) * dst_pitch_px + dx;
                if (blend_child) {
                    for (int32_t col = 0; col < w; col++) {
                        dr[col] = blend_premul_pixel(dr[col], sr[col]);
                    }
                } else {
                    memcpy(dr, sr, (size_t)w * 4u);
                }
            }
        }
    }
    if (swm->cursor_buffer != NULL && swm->cursor_visible) {
        draw_cursor_image(swm, &target, swm->mouse_x, swm->mouse_y);
    } else {
        draw_cursor(&target, swm->mouse_x, swm->mouse_y, swm->current_cursor);
    }

    if (out_region != NULL) {
        out_region->x = target.cx0;
        out_region->y = target.cy0;
        out_region->w = target.cx1 - target.cx0;
        out_region->h = target.cy1 - target.cy0;
    }
    if (out_valid != NULL) *out_valid = 1;

    memcpy(swm->paint, cur, sizeof(cur));
    cursor_commit(swm, &crect);
    swm->damage_valid = 0;
    swm->damage_full = 0;
    for (int i = 0; i < SWM_MAX_SURFACES; i++) {
        swm->surfaces[i].damage_valid = 0;
    }
}

#define GS(a,b,c,d,e,f,g) { a,b,c,d,e,f,g }
static const uint8_t SWM_FONT_5x7[95][7] = {
    GS(0x00,0x00,0x00,0x00,0x00,0x00,0x00), GS(0x04,0x04,0x04,0x04,0x04,0x00,0x04),
    GS(0x0A,0x0A,0x00,0x00,0x00,0x00,0x00), GS(0x0A,0x1F,0x0A,0x0A,0x0A,0x1F,0x0A),
    GS(0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04), GS(0x19,0x19,0x02,0x04,0x08,0x13,0x13),
    GS(0x0C,0x12,0x14,0x08,0x15,0x12,0x0D), GS(0x04,0x04,0x00,0x00,0x00,0x00,0x00),
    GS(0x02,0x04,0x08,0x08,0x08,0x04,0x02), GS(0x08,0x04,0x02,0x02,0x02,0x04,0x08),
    GS(0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00), GS(0x00,0x04,0x04,0x1F,0x04,0x04,0x00),
    GS(0x00,0x00,0x00,0x00,0x00,0x04,0x08), GS(0x00,0x00,0x00,0x1F,0x00,0x00,0x00),
    GS(0x00,0x00,0x00,0x00,0x00,0x00,0x04), GS(0x01,0x02,0x02,0x04,0x08,0x08,0x10),
    GS(0x0E,0x11,0x13,0x15,0x19,0x11,0x0E), GS(0x04,0x0C,0x04,0x04,0x04,0x04,0x0E),
    GS(0x0E,0x11,0x01,0x02,0x04,0x08,0x1F), GS(0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E),
    GS(0x02,0x06,0x0A,0x12,0x1F,0x02,0x02), GS(0x1F,0x10,0x1E,0x01,0x01,0x01,0x1E),
    GS(0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E), GS(0x1F,0x01,0x02,0x04,0x08,0x10,0x10),
    GS(0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E), GS(0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E),
    GS(0x00,0x04,0x00,0x00,0x00,0x04,0x00), GS(0x00,0x04,0x00,0x00,0x00,0x04,0x08),
    GS(0x01,0x02,0x04,0x08,0x04,0x02,0x01), GS(0x00,0x00,0x1F,0x00,0x1F,0x00,0x00),
    GS(0x10,0x08,0x04,0x02,0x04,0x08,0x10), GS(0x0E,0x11,0x01,0x02,0x04,0x00,0x04),
    GS(0x0E,0x11,0x17,0x15,0x17,0x10,0x0E), GS(0x0E,0x11,0x11,0x1F,0x11,0x11,0x11),
    GS(0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E), GS(0x0F,0x10,0x10,0x10,0x10,0x10,0x0F),
    GS(0x1E,0x11,0x11,0x11,0x11,0x11,0x1E), GS(0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F),
    GS(0x1F,0x10,0x10,0x1E,0x10,0x10,0x10), GS(0x0F,0x10,0x10,0x13,0x11,0x11,0x0F),
    GS(0x11,0x11,0x11,0x1F,0x11,0x11,0x11), GS(0x0E,0x04,0x04,0x04,0x04,0x04,0x0E),
    GS(0x01,0x01,0x01,0x01,0x01,0x11,0x0E), GS(0x11,0x12,0x14,0x18,0x14,0x12,0x11),
    GS(0x10,0x10,0x10,0x10,0x10,0x10,0x1F), GS(0x11,0x1B,0x15,0x15,0x11,0x11,0x11),
    GS(0x11,0x19,0x15,0x13,0x11,0x11,0x11), GS(0x0E,0x11,0x11,0x11,0x11,0x11,0x0E),
    GS(0x1E,0x11,0x11,0x1E,0x10,0x10,0x10), GS(0x0E,0x11,0x11,0x11,0x15,0x12,0x0D),
    GS(0x1E,0x11,0x11,0x1E,0x14,0x12,0x11), GS(0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E),
    GS(0x1F,0x04,0x04,0x04,0x04,0x04,0x04), GS(0x11,0x11,0x11,0x11,0x11,0x11,0x0E),
    GS(0x11,0x11,0x11,0x11,0x11,0x0A,0x04), GS(0x11,0x11,0x11,0x11,0x15,0x15,0x0A),
    GS(0x11,0x11,0x0A,0x04,0x0A,0x11,0x11), GS(0x11,0x11,0x0A,0x04,0x04,0x04,0x04),
    GS(0x1F,0x01,0x02,0x04,0x08,0x10,0x1F), GS(0x0E,0x08,0x08,0x08,0x08,0x08,0x0E),
    GS(0x10,0x08,0x08,0x04,0x02,0x02,0x01), GS(0x0E,0x02,0x02,0x02,0x02,0x02,0x0E),
    GS(0x04,0x0A,0x11,0x00,0x00,0x00,0x00), GS(0x00,0x00,0x00,0x00,0x00,0x00,0x1F),
    GS(0x08,0x04,0x00,0x00,0x00,0x00,0x00), GS(0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F),
    GS(0x10,0x10,0x1E,0x11,0x11,0x11,0x1E), GS(0x00,0x00,0x0F,0x10,0x10,0x10,0x0F),
    GS(0x01,0x01,0x0F,0x11,0x11,0x11,0x0F), GS(0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E),
    GS(0x06,0x09,0x08,0x1C,0x08,0x08,0x08), GS(0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E),
    GS(0x10,0x10,0x1E,0x11,0x11,0x11,0x11), GS(0x04,0x00,0x0C,0x04,0x04,0x04,0x0E),
    GS(0x02,0x00,0x06,0x02,0x02,0x12,0x0C), GS(0x10,0x10,0x12,0x14,0x18,0x14,0x12),
    GS(0x0C,0x04,0x04,0x04,0x04,0x04,0x0E), GS(0x00,0x00,0x1A,0x15,0x15,0x15,0x15),
    GS(0x00,0x00,0x1E,0x11,0x11,0x11,0x11), GS(0x00,0x00,0x0E,0x11,0x11,0x11,0x0E),
    GS(0x00,0x00,0x1E,0x11,0x1E,0x10,0x10), GS(0x00,0x00,0x0F,0x11,0x0F,0x01,0x01),
    GS(0x00,0x00,0x16,0x19,0x10,0x10,0x10), GS(0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E),
    GS(0x08,0x08,0x1C,0x08,0x08,0x09,0x06), GS(0x00,0x00,0x11,0x11,0x11,0x11,0x0F),
    GS(0x00,0x00,0x11,0x11,0x11,0x0A,0x04), GS(0x00,0x00,0x11,0x11,0x15,0x15,0x0A),
    GS(0x00,0x00,0x11,0x0A,0x04,0x0A,0x11), GS(0x00,0x00,0x11,0x11,0x0F,0x01,0x0E),
    GS(0x00,0x00,0x1F,0x02,0x04,0x08,0x1F), GS(0x02,0x04,0x04,0x08,0x04,0x04,0x02),
    GS(0x04,0x04,0x04,0x04,0x04,0x04,0x04), GS(0x08,0x04,0x04,0x02,0x04,0x04,0x08),
    GS(0x09,0x15,0x12,0x00,0x00,0x00,0x00),
};
