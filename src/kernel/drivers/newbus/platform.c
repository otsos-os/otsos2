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

$define %type device_t as pointer to newbus device
$define %type driver_t as newbus driver descriptor
$define %type int as 32 bit signed

$define %func fw_acpi_identify as procedure with args driver_t *, device_t
$define %func fw_acpi_attach as function with args device_t
$define %func platform_lapic_identify as procedure with args driver_t *, device_t
$define %func platform_lapic_probe as function with args device_t
$define %func platform_lapic_attach as function with args device_t
$define %func platform_ioapic_attach as function with args device_t
$define %func platform_smp_attach as function with args device_t

*/

/* !SPACE!

$space %internal fw_acpi_identify, fw_acpi_attach
$space %internal platform_lapic_identify, platform_lapic_probe
$space %internal platform_lapic_attach, platform_ioapic_attach
$space %internal platform_smp_attach

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/interrupts/apic/ioapic.h>
#include <kernel/interrupts/apic/lapic.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <kernel/smp/smp.h>

static void
fw_acpi_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "acpi", 0) == NULL) {
		device_add_child(parent, "acpi", 0);
	}
}

static int
fw_acpi_probe(device_t dev)
{
	const newbus_bootinfo_t	*boot;

	(void)dev;
	boot = newbus_get_bootinfo();
	if (boot == NULL || boot->magic != MULTIBOOT2_BOOTLOADER_MAGIC ||
	    boot->mb2 == NULL) {
		return (-1);
	}
	return (100);
}

static int
fw_acpi_attach(device_t dev)
{
	const newbus_bootinfo_t	*boot;

	(void)dev;
	boot = newbus_get_bootinfo();
	if (boot == NULL || boot->mb2 == NULL) {
		return (-1);
	}
	return (acpi_init_from_multiboot2(boot->mb2));
}

static void
platform_lapic_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "lapic", 0) == NULL) {
		device_add_child(parent, "lapic", 0);
	}
	if (device_find_child(parent, "ioapic", 0) == NULL) {
		device_add_child(parent, "ioapic", 0);
	}
	if (device_find_child(parent, "smp", 0) == NULL) {
		device_add_child(parent, "smp", 0);
	}
}

static int
platform_lapic_probe(device_t dev)
{
	const newbus_bootinfo_t	*boot;

	(void)dev;
	boot = newbus_get_bootinfo();
	if (boot != NULL && boot->disable_apic) {
		return (-1);
	}
	return (100);
}

static int
platform_lapic_attach(device_t dev)
{
	(void)dev;
	return (lapic_init());
}

static int
platform_ioapic_probe(device_t dev)
{
	const newbus_bootinfo_t	*boot;

	(void)dev;
	boot = newbus_get_bootinfo();
	if (boot != NULL && boot->disable_apic) {
		return (-1);
	}
	if (!acpi_is_initialized()) {
		return (-1);
	}
	return (90);
}

static int
platform_ioapic_attach(device_t dev)
{
	(void)dev;
	return (ioapic_init());
}

static int
platform_smp_probe(device_t dev)
{
	(void)dev;
	return (50);
}

static int
platform_smp_attach(device_t dev)
{
	const newbus_bootinfo_t	*boot;

	(void)dev;
	boot = newbus_get_bootinfo();
	if (boot != NULL && boot->disable_apic) {
		smp_init_single_cpu();
		return (0);
	}
	smp_init();
	return (0);
}

static devclass_t acpi_devclass = { "acpi", 1 };
static devclass_t lapic_devclass = { "lapic", 1 };
static devclass_t ioapic_devclass = { "ioapic", 1 };
static devclass_t smp_devclass = { "smp", 1 };

static driver_t acpi_driver = {
	.name = "acpi",
	.identify = fw_acpi_identify,
	.probe = fw_acpi_probe,
	.attach = fw_acpi_attach,
};

static driver_t lapic_driver = {
	.name = "lapic",
	.identify = platform_lapic_identify,
	.probe = platform_lapic_probe,
	.attach = platform_lapic_attach,
};

static driver_t ioapic_driver = {
	.name = "ioapic",
	.identify = NULL,
	.probe = platform_ioapic_probe,
	.attach = platform_ioapic_attach,
};

static driver_t smp_driver = {
	.name = "smp",
	.identify = NULL,
	.probe = platform_smp_probe,
	.attach = platform_smp_attach,
};

FIRMWARE_DRIVER_MODULE(acpi, acpi_driver, acpi_devclass,
    NEWBUS_PASS_FIRMWARE, NEWBUS_ORDER_FIRST);
PLATFORM_DRIVER_MODULE(lapic, lapic_driver, lapic_devclass,
    NEWBUS_PASS_INTERRUPT, NEWBUS_ORDER_FIRST);
PLATFORM_DRIVER_MODULE(ioapic, ioapic_driver, ioapic_devclass,
    NEWBUS_PASS_INTERRUPT, NEWBUS_ORDER_MIDDLE);
PLATFORM_DRIVER_MODULE(smp, smp_driver, smp_devclass,
    NEWBUS_PASS_CORE, NEWBUS_ORDER_LATE);
