/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type int as 32 bit signed
$define %type device_t as newbus device handle
$define %type driver_t as newbus driver descriptor
$define %type devclass_t as newbus device class

$define %func aml_bus_identify as procedure with args driver_t *, device_t
$define %func aml_bus_probe as function with args device_t
$define %func aml_bus_attach as function with args device_t

*/

/* !SPACE!

$space %internal aml_bus_identify, aml_bus_probe, aml_bus_attach

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/acpi/aml.h>
#include <kernel/drivers/acpi/prt.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static void
aml_bus_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "aml", 0) == NULL) {
		device_add_child(parent, "aml", 0);
	}
}

static int
aml_bus_probe(device_t dev)
{
	(void)dev;
	if (!acpi_is_initialized()) {
		return (-1);
	}
	if (acpi_find_table("DSDT") == NULL &&
	    acpi_get_fadt() == NULL) {
		return (-1);
	}
	return (90);
}

static int
aml_bus_attach(device_t dev)
{
	int	routed;
	int	status;

	(void)dev;
	status = aml_init();
	if (status != 0) {
		return (status);
	}
	routed = acpi_prt_rewire();
	if (routed > 0) {
		drivers_log("aml: _PRT routed %d PCI interrupt(s)\n", routed);
	}
	return (0);
}

static devclass_t aml_devclass = { "aml", 1 };

static driver_t aml_driver = {
	.name = "aml",
	.identify = aml_bus_identify,
	.probe = aml_bus_probe,
	.attach = aml_bus_attach,
	.detach = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.priv = NULL,
};

FIRMWARE_DRIVER_MODULE(aml, aml_driver, aml_devclass,
    NEWBUS_PASS_FIRMWARE, NEWBUS_ORDER_MIDDLE);
