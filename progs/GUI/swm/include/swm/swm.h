/* !DEFINES!

$define %type swm_client as native Sprot client state
$define %type swm_surface as compositor surface state
$define %type swm_state as compositor global state
$define %type swm_interact as active compositor interaction
$define %type swm_rect as signed screen-space rectangle
$define %type swm_paint_record as per-slot record of the last painted frame

*/

/* !SPACE!

$space %export swm_client_t, swm_surface_t, swm_state_t, swm_interact_t
$space %export swm_rect_t, swm_paint_record_t

*/

/*
 * Copyright (c) 2026, otsos team
 */

#ifndef ONETOOL_LIBS_GUI_SWM_INTERNAL_H
#define ONETOOL_LIBS_GUI_SWM_INTERNAL_H

#include <srapi.h>
#include <sprot/sprot.h>

#include <stddef.h>
#include <stdint.h>

#define SWM_MAX_CLIENTS		16
#define SWM_MAX_SURFACES	32
#define SWM_TITLEBAR_H		22
#define SWM_BORDER		2
#define SWM_BTN_SIZE		14

typedef struct swm_client swm_client_t;
typedef struct swm_output swm_output_t;
typedef struct swm_buffer swm_buffer_t;

typedef enum swm_interact {
	SWM_INTERACT_NONE = 0,
	SWM_INTERACT_MOVE = 1
} swm_interact_t;
typedef struct swm_rect {
	int32_t		x;
	int32_t		y;
	int32_t		w;
	int32_t		h;
} swm_rect_t;

typedef struct swm_paint_record {
	swm_rect_t	outer;
	const void	*pixels;
	uint32_t	content_serial;
	uint32_t	title_hash;
	int		painted;
	int		focused;
	int		has_chrome;
} swm_paint_record_t;

typedef struct swm_surface {
	swm_client_t	*owner;
	swm_buffer_t	*buffer;
	size_t		buffer_size;
	uint32_t	id;
	uint32_t	client_handle;
	uint32_t	width;
	uint32_t	height;
	uint32_t	stride;
	uint32_t	role;
	uint32_t	parent_id;
	uint32_t	saved_width;
	uint32_t	saved_height;
	int32_t		rel_x;
	int32_t		rel_y;
	int32_t		pos_x;
	int32_t		pos_y;
	int32_t		saved_pos_x;
	int32_t		saved_pos_y;
	int		in_use;
	int		buffer_has_alpha;
	int		has_pending;
	int		committed;
	int		visible;
	int		minimized;
	int		maximized;
	int		wants_frame;
	int		z;
	swm_rect_t	damage;
	int		damage_valid;
	uint32_t	content_serial;
	char		title[128];
} swm_surface_t;

struct swm_client {
	swm_surface_t	*surfaces[8];
	uint64_t	peer;
	uint32_t	pid;
	uint32_t	uid;
	uint32_t	gid;
	int		in_use;
	int		has_hello;
	int		has_cred;
	int		is_shell;
	int		surface_count;
};

typedef struct swm_state {
	swm_output_t	*output;
	swm_buffer_t	*cursor_buffer;
	swm_surface_t	*grab_surface;
	swm_surface_t	*hovered_surface;
	swm_client_t	*shell_client;
	swm_surface_t	*shell_surface;
	uint64_t	frame_count;
	uint32_t	display_w;
	uint32_t	display_h;
	uint32_t	current_cursor;
	uint32_t	modifiers;
	uint32_t	mouse_buttons;
	uint32_t	next_surface_id;
	int32_t		mouse_x;
	int32_t		mouse_y;
	int32_t		input_raw_x;
	int32_t		input_raw_y;
	int32_t		cursor_hotspot_x;
	int32_t		cursor_hotspot_y;
	int32_t		grab_offset_x;
	int32_t		grab_offset_y;
	int32_t		next_cascade_x;
	int32_t		next_cascade_y;
	int		ipc;
	int		kq;
	int		cursor_visible;
	int		input_have_raw;
	int		mouse_left_down;
	int		interaction;
	int		should_quit;
	int		next_z;
	int		shell_dirty;
	swm_rect_t	damage;
	int		damage_valid;
	int		damage_full;
	swm_rect_t	cursor_prev;
	const void	*cursor_prev_pixels;
	uint32_t	cursor_prev_type;
	int		cursor_prev_valid;
	int		cursor_prev_visible;
	swm_client_t	clients[SWM_MAX_CLIENTS];
	swm_surface_t	surfaces[SWM_MAX_SURFACES];
	swm_paint_record_t paint[SWM_MAX_SURFACES];
} swm_state_t;

#endif
