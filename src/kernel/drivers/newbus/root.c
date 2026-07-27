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

$define %type newbus_bootinfo_t as boot-discovered firmware resource state
$define %type device_t as pointer to newbus device

$define %func newbus_bootstrap as procedure with args newbus_bootinfo_t *
$define %func newbus_update_bootinfo as procedure with args newbus_bootinfo_t *
$define %func newbus_get_bootinfo as function with args void
$define %func root_attach as function with args device_t

*/

/* !SPACE!

$space %internal root_attach
$space %export newbus_bootstrap, newbus_update_bootinfo
$space %export newbus_get_bootinfo

*/

#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static newbus_bootinfo_t	newbus_bootinfo;
static int			newbus_bootinfo_valid;

static int
root_attach(device_t dev)
{
	device_add_child(dev, "nexus", 0);
	return (0);
}

static devclass_t	root_devclass = {
	.name = "root",
	.maxunit = 1,
};

static driver_t	root_driver = {
	.name = "root",
	.identify = NULL,
	.probe = NULL,
	.attach = root_attach,
	.detach = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.priv = NULL,
};

void
newbus_update_bootinfo(newbus_bootinfo_t *bootinfo)
{
	if (bootinfo != NULL) {
		newbus_bootinfo = *bootinfo;
		newbus_bootinfo_valid = 1;
	}
}

const newbus_bootinfo_t *
newbus_get_bootinfo(void)
{
	if (!newbus_bootinfo_valid) {
		return (NULL);
	}
	return (&newbus_bootinfo);
}

void
newbus_bootstrap(newbus_bootinfo_t *bootinfo)
{
	device_t	root;

	newbus_update_bootinfo(bootinfo);
	root = newbus_device_create_root("root", 0);
	if (root == NULL) {
		return;
	}
	newbus_device_set_driver(root, &root_driver, &root_devclass);
	root_attach(root);
	newbus_device_set_state(root, DS_ATTACHED);
	drivers_log("[NEWBUS] bootstrap root0\n");
}
