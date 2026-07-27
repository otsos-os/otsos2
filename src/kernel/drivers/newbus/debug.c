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

$define %type device_t as pointer to newbus device
$define %type int as 32 bit signed

$define %func newbus_dump_tree as procedure with args void
$define %func newbus_dump_drivers as procedure with args void

*/

/* !SPACE!

$space %internal newbus_dump_device
$space %export newbus_dump_tree, newbus_dump_drivers

*/

#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>

static void
newbus_dump_device(device_t dev, int depth)
{
	device_t	child;
	int	i;

	for (i = 0; i < depth; i++) {
		drivers_log("  ");
	}
	drivers_log("%s state=%d driver=%s\n", device_get_nameunit(dev),
	    device_get_state(dev), device_get_driver(dev) != NULL ?
	    device_get_driver(dev)->name : "none");
	for (child = device_get_child(dev); child != NULL;
	    child = device_get_next(child)) {
		newbus_dump_device(child, depth + 1);
	}
}

void
newbus_dump_tree(void)
{
	device_t	dev;
	int	i;

	drivers_log("[NEWBUS] device tree\n");
	for (i = 0; i < newbus_device_count_get(); i++) {
		dev = newbus_device_get(i);
		if (device_get_parent(dev) == NULL) {
			newbus_dump_device(dev, 0);
		}
	}
}

void
newbus_dump_drivers(void)
{
	const newbus_module_t	*module;
	int			i;

	drivers_log("[NEWBUS] driver modules\n");
	for (i = 0; i < newbus_driver_count_get(); i++) {
		module = newbus_driver_module_get(i);
		drivers_log("  %s on %s pass=%d order=%d\n",
		    module->driver->name, module->bus_name,
		    module->pass, module->order);
	}
}
