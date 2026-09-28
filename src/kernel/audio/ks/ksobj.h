/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type ks_object_t as base of every object in the streaming graph
$define %type ks_object_type_t as vtable describing a concrete object kind
$define %type ks_device_t as one audio device (filter factory in KS terms)
$define %type ks_pin_t as one directional pin on a device
$define %type ks_node_t as one topology node inside a device graph

$const KS_OBJ_FLAG_* as lifecycle/visibility flags on a KS object
$const KS_DEVICE_NAME_MAX as longest device/pin/node name

$define %func ks_object_init as procedure with args ks_object_t *, ks_object_type_t *, uint
$define %func ks_object_ref as procedure with args ks_object_t *
$define %func ks_object_unref as procedure with args ks_object_t *
$define %func ks_device_create as function with args const char *, void *
$define %func ks_device_destroy as procedure with args ks_device_t *
$define %func ks_device_add_pin as function with args ks_device_t *, ks_pin_t *
$define %func ks_device_find_pin as function with args ks_device_t *, uint
$define %func ks_device_add_node as function with args ks_device_t *, ks_node_t *
*/

/* !SPACE!

$space %export ks_object_init, ks_object_ref, ks_object_unref
$space %export ks_device_create, ks_device_destroy
$space %export ks_device_add_pin, ks_device_find_pin
$space %export ks_device_add_node, ks_device_pin_count

*/

#ifndef KERNEL_AUDIO_KS_KSOBJ_H
#define KERNEL_AUDIO_KS_KSOBJ_H

#include <mlibc/mlibc.h>
#include <kernel/audio/ks/kstypes.h>

#define	KS_DEVICE_NAME_MAX	64
#define	KS_MAX_PINS_PER_DEVICE	32
#define	KS_MAX_NODES_PER_DEVICE	64

#define	KS_OBJ_FLAG_VALID	0x00000001
#define	KS_OBJ_FLAG_PUBLIC	0x00000002

typedef struct ks_object ks_object_t;

typedef struct ks_object_type {
	const char	*name;
	void		(*release)(ks_object_t *obj);
} ks_object_type_t;

struct ks_object {
	ks_object_type_t	*type;
	ks_object_t		*parent;
	u32			flags;
	s32			refs;
	u32			id;
	char			name[KS_DEVICE_NAME_MAX];
};

typedef struct ks_pin {
	ks_object_t		obj;
	struct ks_device	*dev;
	ks_dataflow_t		flow;
	ks_state_t		state;
	ks_format_t		format;
	u32			pin_id;
	u32			formats_count;
	const ks_format_t	*formats;
} ks_pin_t;

typedef struct ks_node {
	ks_object_t		obj;
	u32			node_id;
	u32			type;
} ks_node_t;

typedef struct ks_device {
	ks_object_t		obj;
	void			*driver_ctx;
	ks_pin_t		*pins[KS_MAX_PINS_PER_DEVICE];
	ks_node_t		*nodes[KS_MAX_NODES_PER_DEVICE];
	u32			pin_count;
	u32			node_count;
} ks_device_t;

void		ks_object_init(ks_object_t *obj, ks_object_type_t *type,
		    u32 id);
void		ks_object_unref(ks_object_t *obj);
ks_object_t	*ks_object_ref(ks_object_t *obj);

ks_device_t	*ks_device_create(const char *name, void *driver_ctx);
void		ks_device_destroy(ks_device_t *dev);
int		ks_device_add_pin(ks_device_t *dev, ks_pin_t *pin);
ks_pin_t	*ks_device_find_pin(ks_device_t *dev, u32 pin_id);
int		ks_device_add_node(ks_device_t *dev, ks_node_t *node);
u32		ks_device_pin_count(const ks_device_t *dev);

#endif
