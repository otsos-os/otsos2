/* !DEFINES!

$define %type de_window as shell window cache entry
$define %type de as native Sprot desktop client
$define %func de_present as target present callback
$define %func de_wait_surface as function with args connection, surface
$define %func de_send_input as function with args desktop, event
$define %func de_handle_event as function with args desktop, event
$define %func de_draw_tooltip as procedure with args desktop
$define %func de_render as procedure with args desktop
$define %func de_set_menu as procedure with args desktop, visible
$define %func de_launch_terminal as procedure with args void
$define %func de_cleanup as procedure with args desktop
$define %func main as start with args void

*/

/* !SPACE!

$space %internal de_present, de_wait_surface, de_send_input, de_handle_event
$space %internal de_window_find, de_window_update, de_window_remove
$space %internal de_format_clock, de_format_stats, de_draw_panel
$space %internal de_draw_menu, de_draw_tooltip, de_launch_terminal
$space %internal de_set_menu, de_cleanup
$space %export main

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#include <libg.h>
#include <native.h>
#include <sprot/client.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DE_TASKBAR_H	32
#define DE_LAUNCHER_W	60
#define DE_STATS_W	132
#define DE_CLOCK_W	54
#define DE_WIN_BTN_W	36
#define DE_TOOLTIP_W	200
#define DE_TOOLTIP_H	24
#define DE_ITEM_GAP	4
#define DE_PADDING	4
#define DE_MAX_WINDOWS	12
#define DE_MENU_W	180
#define DE_MENU_ITEM_H	22
#define DE_MENU_ENTRIES	5
#define DE_FRAME_MS	16

#define DE_COLOR_BAR		0xff12141aU
#define DE_COLOR_BORDER		0xff465f82U
#define DE_COLOR_IDLE		0xff262c3aU
#define DE_COLOR_HOVER		0xff3a465cU
#define DE_COLOR_ACTIVE	0xff3c6eb4U
#define DE_COLOR_TEXT		0xffe6e8f0U
#define DE_COLOR_MUTED		0xff8c919eU
#define DE_COLOR_ACCENT	0xff78c8ffU
#define DE_COLOR_MENU		0xff1e222cU
#define DE_COLOR_DANGER	0xffe66a5cU

struct de_window {
	uint32_t	id;
	uint32_t	state;
	int32_t	x;
	int32_t	y;
	uint32_t	width;
	uint32_t	height;
	char		title[64];
};

struct de {
	sprot_connection_t	*connection;
	sprot_surface_t		*panel;
	sprot_surface_t		*menu;
	sprot_surface_t		*tooltip;
	libg_context_t		*panel_ui;
	libg_context_t		*menu_ui;
	libg_context_t		*tooltip_ui;
	uint32_t		width;
	uint32_t		height;
	int			menu_open;
	int			panel_dirty;
	int			menu_dirty;
	int			tooltip_dirty;
	int32_t		panel_mouse_x;
	int32_t		panel_mouse_y;
	uint32_t		panel_buttons;
	int32_t		menu_mouse_x;
	int32_t		menu_mouse_y;
	uint32_t		menu_buttons;
	struct de_window	windows[DE_MAX_WINDOWS];
	int			window_count;
	uint64_t		last_clock_minute;
	uint64_t		last_stats_ms;
	uint64_t		last_frame_ms;
	char			clock_text[8];
	char			stats_text[40];
	int			tooltip_window;
	int			tooltip_open;
	libg_anim_t		tooltip_anim;
};

static void
de_set_menu(struct de *desktop, int visible)
{
	desktop->menu_open = visible != 0;
	if (desktop->menu != NULL) {
		(void)sprot_set_visible(desktop->menu, desktop->menu_open);
	}
	desktop->menu_dirty = 1;
}

static void
de_cleanup(struct de *desktop)
{
	if (desktop == NULL) {
		return;
	}
	libgDestroy(desktop->tooltip_ui);
	libgDestroy(desktop->menu_ui);
	libgDestroy(desktop->panel_ui);
	if (desktop->tooltip != NULL) {
		sprot_destroy_surface(desktop->tooltip);
	}
	if (desktop->menu != NULL) {
		sprot_destroy_surface(desktop->menu);
	}
	if (desktop->panel != NULL) {
		sprot_destroy_surface(desktop->panel);
	}
	if (desktop->connection != NULL) {
		sprot_disconnect(desktop->connection);
	}
}

static int
de_present(void *userdata, const struct srapi_region *region)
{
	sprot_surface_t *surface;
	uint32_t width, height;

	surface = (sprot_surface_t *)userdata;
	if (surface == NULL) {
		return (-1);
	}
	width = sprot_surface_width(surface);
	height = sprot_surface_height(surface);
	if (region != NULL) {
		if (sprot_damage(surface, region->x, region->y, region->width,
		    region->height) != 0) {
			return (-1);
		}
	} else if (sprot_damage(surface, 0, 0, width, height) != 0) {
		return (-1);
	}
	return (sprot_commit(surface));
}

static int
de_wait_surface(sprot_connection_t *connection, sprot_surface_t *surface)
{
	sprot_event_t event;
	int i, ret;

	for (i = 0; i < 100; i++) {
		if (sprot_surface_id(surface) != 0) {
			return (0);
		}
		ret = sprot_poll_event(connection, &event, 20);
		if (ret < 0) {
			return (-1);
		}
		if (ret == 0) {
			continue;
		}
		if (event.kind == SPROT_EVENT_SURFACE_CREATED &&
		    sprot_surface_id(surface) != 0) {
			return (0);
		}
		if (event.kind == SPROT_EVENT_DISCONNECT) {
			return (-1);
		}
	}
	return (-1);
}

static struct de_window *
de_window_find(struct de *desktop, uint32_t id)
{
	int i;

	for (i = 0; i < desktop->window_count; i++) {
		if (desktop->windows[i].id == id) {
			return (&desktop->windows[i]);
		}
	}
	return (NULL);
}

static void
de_window_update(struct de *desktop, const sprot_body_shell_window_t *body)
{
	struct de_window *window;

	window = de_window_find(desktop, body->id);
	if (window == NULL) {
		if (desktop->window_count >= DE_MAX_WINDOWS) {
			return;
		}
		window = &desktop->windows[desktop->window_count++];
		memset(window, 0, sizeof(*window));
	} else if (window->id == body->id &&
	    window->state == body->state &&
	    window->x == body->x &&
	    window->y == body->y &&
	    window->width == body->width &&
	    window->height == body->height &&
	    strncmp(window->title, body->title, sizeof(window->title)) == 0) {
		return;
	}
	window->id = body->id;
	window->state = body->state;
	window->x = body->x;
	window->y = body->y;
	window->width = body->width;
	window->height = body->height;
	memcpy(window->title, body->title, sizeof(window->title));
	window->title[sizeof(window->title) - 1] = '\0';
	desktop->panel_dirty = 1;
}

static void
de_window_remove(struct de *desktop, uint32_t id)
{
	struct de_window *window;
	int index;

	window = de_window_find(desktop, id);
	if (window == NULL) {
		return;
	}
	index = (int)(window - desktop->windows);
	if (index + 1 < desktop->window_count) {
		memmove(window, window + 1,
		    (size_t)(desktop->window_count - index - 1) *
		    sizeof(*window));
	}
	desktop->window_count--;
	desktop->panel_dirty = 1;
}

static void
de_format_clock(struct de *desktop, const struct api_timeinfo *timeinfo)
{
	uint64_t minute, seconds;

	minute = timeinfo->local_sec / 60;
	if (minute == desktop->last_clock_minute && desktop->clock_text[0] != '\0') {
		return;
	}
	desktop->last_clock_minute = minute;
	seconds = timeinfo->local_sec % 86400;
	(void)snprintf(desktop->clock_text, sizeof(desktop->clock_text),
	    "%02llu:%02llu", (unsigned long long)(seconds / 3600),
	    (unsigned long long)((seconds / 60) % 60));
	desktop->panel_dirty = 1;
}

static void
de_format_stats(struct de *desktop, uint64_t now_ms)
{
	struct api_cpuinfo cpu;
	struct api_meminfo memory;
	uint64_t used, percent;

	if (desktop->last_stats_ms != 0 && now_ms - desktop->last_stats_ms < 500) {
		return;
	}
	desktop->last_stats_ms = now_ms;
	memset(&cpu, 0, sizeof(cpu));
	memset(&memory, 0, sizeof(memory));
	if (sysCpuInfo(&cpu) != 0 || sysMemInfo(&memory) != 0 ||
	    memory.ram_total_kb == 0) {
		(void)snprintf(desktop->stats_text, sizeof(desktop->stats_text),
		    "CPU -- RAM --");
	} else {
		used = memory.ram_total_kb -
		    (memory.ram_free_kb > memory.ram_total_kb ?
		    memory.ram_total_kb : memory.ram_free_kb);
		percent = (used * 100 + memory.ram_total_kb / 2) /
		    memory.ram_total_kb;
		(void)snprintf(desktop->stats_text, sizeof(desktop->stats_text),
		    "CPU %u RAM %llu%%", cpu.cpu_count,
		    (unsigned long long)percent);
	}
	desktop->panel_dirty = 1;
}

static int
de_send_input(struct de *desktop, const sprot_event_t *event)
{
	struct srapi_input_event input;
	int is_menu;

	memset(&input, 0, sizeof(input));
	is_menu = event->object_id == sprot_surface_id(desktop->menu);
	if (event->kind == SPROT_EVENT_POINTER_MOTION ||
	    event->kind == SPROT_EVENT_POINTER_ENTER) {
		input.type = SRAPI_INPUT_MOUSE;
		input.flags = SRAPI_MOUSE_MOVE | SRAPI_MOUSE_ABSOLUTE;
		input.x = event->u.pointer_motion.x;
		input.y = event->u.pointer_motion.y;
		input.buttons = is_menu ? desktop->menu_buttons : desktop->panel_buttons;
		if (is_menu) {
			input.dx = input.x - desktop->menu_mouse_x;
			input.dy = input.y - desktop->menu_mouse_y;
			desktop->menu_mouse_x = input.x;
			desktop->menu_mouse_y = input.y;
			(void)libgHandleInput(desktop->menu_ui, &input);
		} else {
			input.dx = input.x - desktop->panel_mouse_x;
			input.dy = input.y - desktop->panel_mouse_y;
			desktop->panel_mouse_x = input.x;
			desktop->panel_mouse_y = input.y;
			(void)libgHandleInput(desktop->panel_ui, &input);
		}
		return (0);
	} else if (event->kind == SPROT_EVENT_POINTER_BUTTON) {
		input.type = SRAPI_INPUT_MOUSE;
		input.flags = SRAPI_MOUSE_BUTTON | SRAPI_MOUSE_ABSOLUTE;
		input.buttons = is_menu ? desktop->menu_buttons : desktop->panel_buttons;
		if (event->u.pointer_button.button == SRAPI_MOUSE_LEFT) {
			if (event->u.pointer_button.state == SPROT_BUTTON_STATE_PRESSED) {
				input.buttons |= SRAPI_MOUSE_LEFT;
			} else {
				input.buttons &= ~SRAPI_MOUSE_LEFT;
			}
		}
		if (is_menu) {
			desktop->menu_buttons = input.buttons;
			input.x = desktop->menu_mouse_x;
			input.y = desktop->menu_mouse_y;
			(void)libgHandleInput(desktop->menu_ui, &input);
		} else {
			desktop->panel_buttons = input.buttons;
			input.x = desktop->panel_mouse_x;
			input.y = desktop->panel_mouse_y;
			(void)libgHandleInput(desktop->panel_ui, &input);
		}
		return (1);
	} else if (event->kind == SPROT_EVENT_KEY) {
		input.type = SRAPI_INPUT_KEYBOARD;
		input.flags = event->u.key.state == SPROT_KEY_STATE_PRESSED ?
		    SRAPI_KEY_PRESS : SRAPI_KEY_RELEASE;
		input.key = event->u.key.scancode;
		input.mods = event->u.key.modifiers;
		(void)libgHandleInput(desktop->panel_ui, &input);
		return (1);
	}
	return (0);
}

static int
de_handle_event(struct de *desktop, const sprot_event_t *event)
{
	int pending;
	pending = 0;
	if (event->kind == SPROT_EVENT_SHELL_WINDOW) {
		de_window_update(desktop, &event->u.shell_window);
	} else if (event->kind == SPROT_EVENT_SHELL_REMOVE) {
		de_window_remove(desktop, event->object_id);
	} else if (event->kind == SPROT_EVENT_POINTER_MOTION ||
	    event->kind == SPROT_EVENT_POINTER_ENTER ||
	    event->kind == SPROT_EVENT_POINTER_BUTTON ||
	    event->kind == SPROT_EVENT_KEY) {
		pending = de_send_input(desktop, event);
		if (event->kind == SPROT_EVENT_KEY &&
		    event->u.key.state == SPROT_KEY_STATE_PRESSED &&
		    (event->u.key.scancode == SRAPI_KEY_LSUPER ||
		    event->u.key.scancode == SRAPI_KEY_RSUPER)) {
			de_set_menu(desktop, !desktop->menu_open);
		}
	} else if (event->kind == SPROT_EVENT_SURFACE_CLOSE &&
	    event->object_id == sprot_surface_id(desktop->menu)) {
		de_set_menu(desktop, 0);
	}
	return (pending);
}

static void
de_draw_panel(struct de *desktop)
{
	const struct de_window *window;
	libg_rect_t rect;
	uint32_t color, state;
	int32_t item_x, text_width, text_height;
	int i, hovered_idx;
	char label[16];

	rect.x = 0;
	rect.y = 0;
	rect.width = (int32_t)desktop->width;
	rect.height = DE_TASKBAR_H;
	libgFillRect(desktop->panel_ui, rect, DE_COLOR_BAR);
	libgStrokeRect(desktop->panel_ui, rect, DE_COLOR_BORDER);
	item_x = DE_LAUNCHER_W + 2 * DE_PADDING;
	rect.x = DE_PADDING;
	rect.y = DE_PADDING;
	rect.width = DE_LAUNCHER_W;
	rect.height = DE_TASKBAR_H - 2 * DE_PADDING;
	state = libgButton(desktop->panel_ui, 1, rect, "MENU");
	if ((state & LIBG_WIDGET_CLICKED) != 0) {
		de_set_menu(desktop, !desktop->menu_open);
	}
	hovered_idx = -1;
	for (i = 0; i < desktop->window_count; i++) {
		window = &desktop->windows[i];
		rect.x = item_x + i * (DE_WIN_BTN_W + DE_ITEM_GAP);
		rect.y = DE_PADDING;
		rect.width = DE_WIN_BTN_W;
		rect.height = DE_TASKBAR_H - 2 * DE_PADDING;
		color = (window->state & SPROT_SURFACE_STATE_FOCUSED) != 0 ?
		    DE_COLOR_ACTIVE : DE_COLOR_IDLE;
		if ((window->state & SPROT_SURFACE_STATE_MINIMIZED) != 0) {
			color = DE_COLOR_BAR;
		}
		libgFillRect(desktop->panel_ui, rect, color);
		snprintf(label, sizeof(label), "#%d", i + 1);
		state = libgButton(desktop->panel_ui, 1000 + (uint32_t)i, rect,
		    label);
		if ((state & LIBG_WIDGET_HOT) != 0) {
			hovered_idx = i;
		}
		if ((state & LIBG_WIDGET_CLICKED) != 0) {
			if ((window->state & SPROT_SURFACE_STATE_FOCUSED) != 0 &&
			    (window->state & SPROT_SURFACE_STATE_MINIMIZED) == 0) {
				(void)sprot_shell_action(desktop->connection,
				    SPROT_SHELL_ACTION_MINIMIZE, window->id);
			} else {
				(void)sprot_shell_action(desktop->connection,
				    SPROT_SHELL_ACTION_FOCUS, window->id);
			}
		}
	}
	if (hovered_idx != desktop->tooltip_window) {
		if (hovered_idx >= 0) {
			int32_t tx;
			tx = item_x + hovered_idx * (DE_WIN_BTN_W + DE_ITEM_GAP) +
			    DE_WIN_BTN_W / 2 - DE_TOOLTIP_W / 2;
			if (tx < 4) {
				tx = 4;
			}
			if (tx + DE_TOOLTIP_W > (int32_t)desktop->width - 4) {
				tx = (int32_t)desktop->width - DE_TOOLTIP_W - 4;
			}
			(void)sprot_set_role(desktop->tooltip,
			    SPROT_SURFACE_ROLE_POPUP,
			    sprot_surface_id(desktop->panel), tx,
			    -(DE_TOOLTIP_H + 4));
			libgAnimStart(&desktop->tooltip_anim, 0, 255, 180,
			    LIBG_EASE_OUT, desktop->last_frame_ms);
			(void)sprot_set_visible(desktop->tooltip, 1);
			desktop->tooltip_open = 1;
			desktop->tooltip_window = hovered_idx;
			desktop->tooltip_dirty = 1;
		} else {
			if (desktop->tooltip_open) {
				(void)sprot_set_visible(desktop->tooltip, 0);
				desktop->tooltip_open = 0;
				desktop->tooltip_window = -1;
			}
		}
	}
	rect.x = (int32_t)desktop->width - DE_STATS_W - DE_CLOCK_W -
	    2 * DE_PADDING;
	rect.y = DE_PADDING;
	rect.width = DE_STATS_W;
	rect.height = DE_TASKBAR_H - 2 * DE_PADDING;
	libgText(desktop->panel_ui, rect.x + 4, rect.y + 8, desktop->stats_text,
	    DE_COLOR_TEXT);
	rect.x = (int32_t)desktop->width - DE_CLOCK_W - DE_PADDING;
	libgFillRect(desktop->panel_ui, rect, DE_COLOR_BAR);
	libgMeasureText(desktop->clock_text, 1, &text_width, &text_height);
	libgText(desktop->panel_ui, rect.x + (rect.width - text_width) / 2,
	    rect.y + (rect.height - text_height) / 2, desktop->clock_text,
	    DE_COLOR_ACCENT);
	(void)libgPresent(desktop->panel_ui);
}

static void
de_draw_tooltip(struct de *desktop)
{
	const struct de_window *window;
	libg_rect_t rect;
	uint32_t bg, border, text_color;
	int32_t alpha, tw, th;
	const char *title;

	if (!desktop->tooltip_open || desktop->tooltip_window < 0 ||
	    desktop->tooltip_window >= desktop->window_count) {
		return;
	}
	window = &desktop->windows[desktop->tooltip_window];
	title = window->title[0] != '\0' ? window->title : "(window)";

	(void)libgAnimUpdate(&desktop->tooltip_anim, desktop->last_frame_ms,
	    &alpha);
	if (desktop->tooltip_anim.active) {
		desktop->tooltip_dirty = 1;
	}

	rect.x = 0;
	rect.y = 0;
	rect.width = DE_TOOLTIP_W;
	rect.height = DE_TOOLTIP_H;

	bg = libgBlendColor(DE_COLOR_BAR, DE_COLOR_MENU, alpha);
	border = libgBlendColor(DE_COLOR_BORDER, DE_COLOR_ACCENT, alpha);
	text_color = libgBlendColor(DE_COLOR_MUTED, DE_COLOR_TEXT, alpha);

	libgFillRect(desktop->tooltip_ui, rect, bg);
	libgStrokeRect(desktop->tooltip_ui, rect, border);
	libgMeasureText(title, 1, &tw, &th);
	libgText(desktop->tooltip_ui, (rect.width - tw) / 2,
	    (rect.height - th) / 2, title, text_color);
	(void)libgPresent(desktop->tooltip_ui);
}

static void
de_launch_terminal(void)
{
	const char *argv[] = { "term", NULL };

	(void)procSpawnNative("/bin/term", (char *const *)argv, NULL);
}

static void
de_draw_menu(struct de *desktop)
{
	static const char *labels[DE_MENU_ENTRIES] = {
		"Terminal", "Tile windows", "Cascade", "Minimize all", "Quit SWM"
	};
	libg_rect_t rect;
	uint32_t state;
	int i;

	if (!desktop->menu_open) {
		return;
	}
	rect.x = 0;
	rect.y = 0;
	rect.width = DE_MENU_W;
	rect.height = DE_MENU_ITEM_H * DE_MENU_ENTRIES + 8;
	libgFillRect(desktop->menu_ui, rect, DE_COLOR_MENU);
	libgStrokeRect(desktop->menu_ui, rect, DE_COLOR_BORDER);
	for (i = 0; i < DE_MENU_ENTRIES; i++) {
		rect.x = 4;
		rect.y = 4 + i * DE_MENU_ITEM_H;
		rect.width = DE_MENU_W - 8;
		rect.height = DE_MENU_ITEM_H - 2;
		state = libgButton(desktop->menu_ui, 100 + (uint32_t)i, rect,
		    labels[i]);
		if ((state & LIBG_WIDGET_CLICKED) != 0) {
			if (i == 0) {
				de_launch_terminal();
			} else {
				(void)sprot_shell_action(desktop->connection,
				    SPROT_SHELL_ACTION_TILE + (uint32_t)(i - 1), 0);
			}
			de_set_menu(desktop, 0);
		}
	}
	(void)libgPresent(desktop->menu_ui);
}

static void
de_render(struct de *desktop)
{
	struct api_timeinfo timeinfo;

	memset(&timeinfo, 0, sizeof(timeinfo));
	if (sysTimeInfo(&timeinfo) == 0) {
		de_format_clock(desktop, &timeinfo);
		de_format_stats(desktop, timeinfo.uptime_sec * 1000 +
			timeinfo.uptime_nsec / 1000000);
	}
	de_draw_panel(desktop);
	de_draw_menu(desktop);
	de_draw_tooltip(desktop);
	desktop->panel_dirty = 0;
	desktop->menu_dirty = 0;
	desktop->tooltip_dirty = 0;
}

int
main(void)
{
	struct api_timeinfo timeinfo;
	struct de desktop;
	libg_style_t style;
	sprot_event_t event;
	uint64_t now_ms;
	int ret;
	int input_pending;
	int disconnected;

	memset(&desktop, 0, sizeof(desktop));
	desktop.tooltip_window = -1;
	desktop.connection = sprot_connect(SPROT_DEFAULT_SERVICE);
	if (desktop.connection == NULL) {
		termPrint("de: cannot connect to swm\n");
		return (1);
	}
	desktop.width = sprot_display_width(desktop.connection);
	desktop.height = sprot_display_height(desktop.connection);
	if (desktop.height <= DE_TASKBAR_H) {
		de_cleanup(&desktop);
		return (1);
	}
	desktop.panel = sprot_create_surface(desktop.connection, desktop.width,
	    DE_TASKBAR_H);
	desktop.menu = sprot_create_surface(desktop.connection, DE_MENU_W,
	    DE_MENU_ITEM_H * DE_MENU_ENTRIES + 8);
	desktop.tooltip = sprot_create_surface(desktop.connection, DE_TOOLTIP_W,
	    DE_TOOLTIP_H);
	if (desktop.panel == NULL || desktop.menu == NULL ||
	    desktop.tooltip == NULL ||
	    de_wait_surface(desktop.connection, desktop.panel) != 0 ||
	    de_wait_surface(desktop.connection, desktop.menu) != 0 ||
	    de_wait_surface(desktop.connection, desktop.tooltip) != 0) {
		termPrint("de: cannot create surfaces\n");
		de_cleanup(&desktop);
		return (1);
	}
	if (sprot_set_role(desktop.panel, SPROT_SURFACE_ROLE_PANEL, 0, 0,
	    (int32_t)desktop.height - DE_TASKBAR_H) != 0 ||
	    sprot_set_role(desktop.menu, SPROT_SURFACE_ROLE_POPUP,
	    sprot_surface_id(desktop.panel), 4,
	    -(DE_MENU_ITEM_H * DE_MENU_ENTRIES + 12)) != 0 ||
	    sprot_set_role(desktop.tooltip, SPROT_SURFACE_ROLE_POPUP,
	    sprot_surface_id(desktop.panel), 0, -(DE_TOOLTIP_H + 4)) != 0 ||
	    sprot_set_visible(desktop.menu, 0) != 0 ||
	    sprot_set_visible(desktop.tooltip, 0) != 0 ||
	    sprot_shell_subscribe(desktop.connection) != 0) {
		termPrint("de: shell registration failed\n");
		de_cleanup(&desktop);
		return (1);
	}
	libgDefaultStyle(&style);
	style.background = DE_COLOR_BAR;
	style.control = DE_COLOR_IDLE;
	style.control_hot = DE_COLOR_HOVER;
	style.control_active = DE_COLOR_ACTIVE;
	style.text = DE_COLOR_TEXT;
	style.text_muted = DE_COLOR_MUTED;
	style.accent = DE_COLOR_ACCENT;
	if (libgCreateForTarget(sprot_surface_pixels(desktop.panel), desktop.width,
	    DE_TASKBAR_H, sprot_surface_stride(desktop.panel), de_present,
	    desktop.panel, &style, &desktop.panel_ui) != LIBG_OK ||
	    libgCreateForTarget(sprot_surface_pixels(desktop.menu), DE_MENU_W,
	    DE_MENU_ITEM_H * DE_MENU_ENTRIES + 8,
	    sprot_surface_stride(desktop.menu), de_present, desktop.menu, &style,
	    &desktop.menu_ui) != LIBG_OK ||
	    libgCreateForTarget(sprot_surface_pixels(desktop.tooltip), DE_TOOLTIP_W,
	    DE_TOOLTIP_H, sprot_surface_stride(desktop.tooltip), de_present,
	    desktop.tooltip, &style, &desktop.tooltip_ui) != LIBG_OK) {
		termPrint("de: LibG initialization failed\n");
		de_cleanup(&desktop);
		return (1);
	}
	desktop.panel_dirty = 1;
	disconnected = 0;
	while (!disconnected) {
		(void)libgBeginOverlay(desktop.panel_ui);
		(void)libgBeginOverlay(desktop.menu_ui);
		(void)libgBeginOverlay(desktop.tooltip_ui);
		input_pending = 0;
		ret = sprot_poll_event(desktop.connection, &event, DE_FRAME_MS);
		if (ret < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			}
			break;
		}
		if (ret > 0) {
			if (event.kind == SPROT_EVENT_DISCONNECT) {
				break;
			}
			input_pending |= de_handle_event(&desktop, &event);
		}
		while ((ret = sprot_poll_event(desktop.connection, &event, 0)) > 0) {
			if (event.kind == SPROT_EVENT_DISCONNECT) {
				disconnected = 1;
				break;
			}
			input_pending |= de_handle_event(&desktop, &event);
		}
		if (ret < 0 && errno != EAGAIN && errno != EINTR) {
			break;
		}
		if (disconnected) {
			break;
		}
		memset(&timeinfo, 0, sizeof(timeinfo));
		if (sysTimeInfo(&timeinfo) != 0) {
			now_ms = desktop.last_frame_ms;
		} else {
			now_ms = timeinfo.uptime_sec * 1000 +
			    timeinfo.uptime_nsec / 1000000;
		}
		if (!input_pending && now_ms - desktop.last_frame_ms < DE_FRAME_MS &&
		    !desktop.panel_dirty && !desktop.menu_dirty &&
		    !desktop.tooltip_dirty) {
			continue;
		}
		desktop.last_frame_ms = now_ms;
		de_render(&desktop);
	}
	de_cleanup(&desktop);
	return (0);
}
