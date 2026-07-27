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

$define %func nexus_attach as function with args device_t
$define %func generic_bus_attach as function with args device_t

*/

/* !SPACE!

$space %internal nexus_attach, generic_bus_attach

*/

#include <kernel/drivers/newbus/newbus.h>

static int
nexus_attach(device_t dev)
{
	device_add_child(dev, "firmware", 0);
	device_add_child(dev, "platform", 0);
	device_add_child(dev, "isa", 0);
	device_add_child(dev, "pseudo", 0);
	return (0);
}

static int
generic_bus_attach(device_t dev)
{
	(void)dev;
	return (0);
}

static devclass_t	nexus_devclass = { "nexus", 1 };
static devclass_t	firmware_devclass = { "firmware", 1 };
static devclass_t	platform_devclass = { "platform", 1 };
static devclass_t	isa_devclass = { "isa", 1 };
static devclass_t	pseudo_devclass = { "pseudo", 1 };

static driver_t	nexus_driver = {
	.name = "nexus",
	.identify = NULL,
	.probe = NULL,
	.attach = nexus_attach,
	.detach = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.priv = NULL,
};

static driver_t	firmware_driver = {
	.name = "firmware",
	.identify = NULL,
	.probe = NULL,
	.attach = generic_bus_attach,
};

static driver_t	platform_driver = {
	.name = "platform",
	.identify = NULL,
	.probe = NULL,
	.attach = generic_bus_attach,
};

static driver_t	isa_driver = {
	.name = "isa",
	.identify = NULL,
	.probe = NULL,
	.attach = generic_bus_attach,
};

static driver_t	pseudo_driver = {
	.name = "pseudo",
	.identify = NULL,
	.probe = NULL,
	.attach = generic_bus_attach,
};

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass,
    NEWBUS_PASS_ROOT, NEWBUS_ORDER_FIRST);
DRIVER_MODULE(firmware, nexus, firmware_driver, firmware_devclass,
    NEWBUS_PASS_BUS, NEWBUS_ORDER_FIRST);
DRIVER_MODULE(platform, nexus, platform_driver, platform_devclass,
    NEWBUS_PASS_BUS, NEWBUS_ORDER_FIRST);
DRIVER_MODULE(isa, nexus, isa_driver, isa_devclass,
    NEWBUS_PASS_BUS, NEWBUS_ORDER_FIRST);
DRIVER_MODULE(pseudo, nexus, pseudo_driver, pseudo_devclass,
    NEWBUS_PASS_BUS, NEWBUS_ORDER_FIRST);
