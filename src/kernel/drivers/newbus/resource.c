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
 * LIABLE FOR ANY DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type resource_t as allocated bus resource
$define %type device_t as pointer to newbus device
$define %type int as 32 bit signed

$define %func bus_set_resource as function with args device_t, int, int, u64, u64, u32
$define %func bus_get_resource as function with args device_t, int, int
$define %func bus_alloc_resource as function with args device_t, int, int *, u64, u64, u32
$define %func bus_release_resource as function with args device_t, int, int, resource_t *

*/

/* !SPACE!

$space %export bus_set_resource, bus_get_resource
$space %export bus_alloc_resource, bus_alloc_resource_any
$space %export bus_release_resource, bus_activate_resource
$space %export bus_deactivate_resource

*/

#include <kernel/drivers/newbus/newbus.h>

int
bus_set_resource(device_t dev, int type, int rid, u64 start,
    u64 count, u32 flags)
{
	resource_t				*res;
	int					i;

	if (dev == NULL) {
		return (-1);
	}
	for (i = 0; i < dev->resource_count; i++) {
		res = &dev->resources[i];
		if (res->type == type && res->rid == rid) {
			res->start = start;
			res->count = count;
			res->flags = flags;
			return (0);
		}
	}
	if (dev->resource_count >= NEWBUS_MAX_RESOURCES) {
		return (-1);
	}
	res = &dev->resources[dev->resource_count++];
	res->owner = dev;
	res->type = type;
	res->rid = rid;
	res->start = start;
	res->count = count;
	res->flags = flags;
	res->cookie = NULL;
	return (0);
}

resource_t *
bus_get_resource(device_t dev, int type, int rid)
{
	int					i;

	if (dev == NULL) {
		return (NULL);
	}
	for (i = 0; i < dev->resource_count; i++) {
		if (dev->resources[i].type == type &&
		    dev->resources[i].rid == rid) {
			return (&dev->resources[i]);
		}
	}
	return (NULL);
}

resource_t *
bus_alloc_resource(device_t dev, int type, int *rid, u64 start,
    u64 count, u32 flags)
{
	resource_t	*res;
	int		local_rid;

	if (dev == NULL || rid == NULL) {
		return (NULL);
	}
	local_rid = *rid;
	res = bus_get_resource(dev, type, local_rid);
	if (res == NULL) {
		if (bus_set_resource(dev, type, local_rid, start,
		    count, 0) != 0) {
			return (NULL);
		}
		res = bus_get_resource(dev, type, local_rid);
	}
	if (res == NULL || (res->flags & RF_BUSY)) {
		return (NULL);
	}
	if (start != 0 || count != 0) {
		res->start = start;
		res->count = count;
	}
	res->flags |= RF_ALLOCATED | RF_BUSY | flags;
	if (flags & RF_ACTIVE) {
		bus_activate_resource(dev, res);
	}
	return (res);
}

resource_t *
bus_alloc_resource_any(device_t dev, int type, int *rid, u32 flags)
{
	resource_t				*res;
	int					i;

	if (dev == NULL || rid == NULL) {
		return (NULL);
	}
	for (i = 0; i < dev->resource_count; i++) {
		res = &dev->resources[i];
		if (res->type != type || (res->flags & RF_BUSY)) {
			continue;
		}
		*rid = res->rid;
		res->flags |= RF_ALLOCATED | RF_BUSY | flags;
		if (flags & RF_ACTIVE) {
			bus_activate_resource(dev, res);
		}
		return (res);
	}
	return (NULL);
}

int
bus_release_resource(device_t dev, int type, int rid, resource_t *res)
{
	if (dev == NULL || res == NULL || res->owner != dev ||
	    res->type != type || res->rid != rid) {
		return (-1);
	}
	res->flags &= ~(RF_BUSY | RF_ALLOCATED | RF_ACTIVE);
	return (0);
}

int
bus_activate_resource(device_t dev, resource_t *res)
{
	if (dev == NULL || res == NULL || res->owner != dev) {
		return (-1);
	}
	res->flags |= RF_ACTIVE;
	return (0);
}

int
bus_deactivate_resource(device_t dev, resource_t *res)
{
	if (dev == NULL || res == NULL || res->owner != dev) {
		return (-1);
	}
	res->flags &= ~RF_ACTIVE;
	return (0);
}
