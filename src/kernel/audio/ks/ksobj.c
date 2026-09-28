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
$define %type ks_device_t as one audio device (filter factory)
$define %type ks_pin_t as one directional pin on a device
$define %type ks_node_t as one topology node inside a device graph

$define %func ks_object_init as procedure with args ks_object_t *, ks_object_type_t *, uint
$define %func ks_object_unref as procedure with args ks_object_t *
$define %func ks_device_create as function with args const char *, void *
$define %func ks_device_destroy as procedure with args ks_device_t *
$define %func ks_device_add_pin as function with args ks_device_t *, ks_pin_t *
$define %func ks_device_find_pin as function with args ks_device_t *, uint
$define %func ks_device_add_node as function with args ks_device_t *, ks_node_t *
*/

/* !SPACE!

$space %export ks_object_init, ks_object_unref, ks_object_ref
$space %export ks_device_create, ks_device_destroy
$space %export ks_device_add_pin, ks_device_find_pin
$space %export ks_device_add_node, ks_device_pin_count

*/

#include <kernel/audio/ks/ksobj.h>

static ks_object_type_t	ks_device_type;
static ks_object_type_t	ks_pin_type;
static ks_object_type_t	ks_node_type;

static void
ks_device_release(ks_object_t *obj)
{
	(void)obj;
}

static void
ks_pin_release(ks_object_t *obj)
{
	(void)obj;
}

static void
ks_node_release(ks_object_t *obj)
{
	(void)obj;
}

void
ks_object_init(ks_object_t *obj, ks_object_type_t *type, u32 id)
{
	if (obj == NULL) {
		return;
	}
	memset(obj, 0, sizeof(*obj));
	obj->type = type;
	obj->flags = KS_OBJ_FLAG_VALID;
	obj->refs = 1;
	obj->id = id;
}

ks_object_t *
ks_object_ref(ks_object_t *obj)
{
	if (obj != NULL && (obj->flags & KS_OBJ_FLAG_VALID)) {
		__atomic_add_fetch(&obj->refs, 1, __ATOMIC_ACQ_REL);
	}
	return (obj);
}

void
ks_object_unref(ks_object_t *obj)
{
	s32	refs;

	if (obj == NULL) {
		return;
	}
	refs = __atomic_sub_fetch(&obj->refs, 1, __ATOMIC_ACQ_REL);
	if (refs == 0) {
		obj->flags &= ~KS_OBJ_FLAG_VALID;
		if (obj->type != NULL && obj->type->release != NULL) {
			obj->type->release(obj);
		}
	}
}

static void
ks_object_types_init(void)
{
	if (ks_device_type.name != NULL) {
		return;
	}
	ks_device_type.name = "device";
	ks_device_type.release = ks_device_release;
	ks_pin_type.name = "pin";
	ks_pin_type.release = ks_pin_release;
	ks_node_type.name = "node";
	ks_node_type.release = ks_node_release;
}

ks_device_t *
ks_device_create(const char *name, void *driver_ctx)
{
	ks_device_t	*dev;
	u32		len;

	ks_object_types_init();
	dev = (ks_device_t *)kmem_calloc(1, sizeof(*dev));
	if (dev == NULL) {
		return (NULL);
	}
	ks_object_init(&dev->obj, &ks_device_type, 0);
	dev->driver_ctx = driver_ctx;
	if (name != NULL) {
		len = strlen(name);
		if (len >= KS_DEVICE_NAME_MAX) {
			len = KS_DEVICE_NAME_MAX - 1;
		}
		memcpy(dev->obj.name, name, len);
		dev->obj.name[len] = '\0';
	}
	return (dev);
}

void
ks_device_destroy(ks_device_t *dev)
{
	u32	i;

	if (dev == NULL) {
		return;
	}
	for (i = 0; i < dev->pin_count; i++) {
		ks_object_unref(&dev->pins[i]->obj);
		dev->pins[i] = NULL;
	}
	for (i = 0; i < dev->node_count; i++) {
		ks_object_unref(&dev->nodes[i]->obj);
		dev->nodes[i] = NULL;
	}
	dev->pin_count = 0;
	dev->node_count = 0;
	ks_object_unref(&dev->obj);
}

int
ks_device_add_pin(ks_device_t *dev, ks_pin_t *pin)
{
	if (dev == NULL || pin == NULL ||
	    dev->pin_count >= KS_MAX_PINS_PER_DEVICE) {
		return (-1);
	}
	ks_object_init(&pin->obj, &ks_pin_type, dev->pin_count);
	pin->dev = dev;
	pin->pin_id = dev->pin_count;
	pin->state = KS_STATE_STOP;
	dev->pins[dev->pin_count++] = pin;
	return (0);
}

ks_pin_t *
ks_device_find_pin(ks_device_t *dev, u32 pin_id)
{
	u32	i;

	if (dev == NULL) {
		return (NULL);
	}
	for (i = 0; i < dev->pin_count; i++) {
		if (dev->pins[i] != NULL &&
		    dev->pins[i]->pin_id == pin_id) {
			return (dev->pins[i]);
		}
	}
	return (NULL);
}

int
ks_device_add_node(ks_device_t *dev, ks_node_t *node)
{
	if (dev == NULL || node == NULL ||
	    dev->node_count >= KS_MAX_NODES_PER_DEVICE) {
		return (-1);
	}
	ks_object_init(&node->obj, &ks_node_type, dev->node_count);
	node->node_id = dev->node_count;
	dev->nodes[dev->node_count++] = node;
	return (0);
}

u32
ks_device_pin_count(const ks_device_t *dev)
{
	return (dev == NULL ? 0 : dev->pin_count);
}
