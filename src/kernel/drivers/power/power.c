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

$define %type u16 as 16 bit unsigned
$define %type int as 32 bit signed
$define %type power_info_t as struct with PM register info and flags

$define %func power_init as function with args void
$define %func power_shutdown as procedure with args void
$define %func power_reboot as procedure with args void
$define %func power_acpi_enable as function with args void
$define %func power_get_info as function with args void
$define %func power_is_initialized as function with args void
$define %func parse_s5_from_dsdt as function with args acpi_fadt_t *

*/

/* !SPACE!

$space %export power_init, power_shutdown, power_reboot
$space %export power_acpi_enable, power_get_info, power_is_initialized
$space %internal parse_s5_from_dsdt

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/drivers/power/power.h>
#include <kernel/panic.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	PM1_SLP_EN	(1 << 13)
#define	PM1_SLP_TYP_MASK	(7 << 10)

#define	KB_CTRL_PORT	0x64
#define	KB_RESET_CMD	0xFE

static power_info_t	g_power;

static int
parse_s5_from_dsdt(acpi_fadt_t *fadt)
{
	acpi_sdt_header_t	*dsdt;
	u8			*aml, *s5_ptr;
	u64			dsdt_addr;
	u32			aml_length, i;

	if (!fadt) {
		return (-1);
	}

	dsdt_addr = 0;
	if (acpi_get_revision() >= 2 && fadt->x_dsdt) {
		dsdt_addr = fadt->x_dsdt;
	} else {
		dsdt_addr = (u64)fadt->dsdt;
	}

	if (!dsdt_addr) {
		drivers_log("[POWER] no DSDT found\n");
		return (-1);
	}

	dsdt = (acpi_sdt_header_t *)dsdt_addr;

	if (dsdt->signature[0] != 'D' || dsdt->signature[1] != 'S' ||
	    dsdt->signature[2] != 'D' || dsdt->signature[3] != 'T') {
		drivers_log("[POWER] invalid DSDT signature\n");
		return (-1);
	}

	drivers_log("[POWER] DSDT at %p, length %u\n", (void *)dsdt_addr,
	    dsdt->length);

	aml = (u8 *)dsdt + sizeof(acpi_sdt_header_t);
	aml_length = dsdt->length - sizeof(acpi_sdt_header_t);

	for (i = 0; i < aml_length - 4; i++) {
		if (aml[i] == '_' && aml[i + 1] == 'S' &&
		    aml[i + 2] == '5' && aml[i + 3] == '_') {
			drivers_log("[POWER] found _S5_ at DSDT "
			    "offset %u\n", i);

			s5_ptr = aml + i + 4;

			if (*s5_ptr == 0x12) {
				u8	pkg_len;

				s5_ptr++;
				pkg_len = *s5_ptr;
				s5_ptr++;
				s5_ptr++;

				if (*s5_ptr == 0x0A) {
					s5_ptr++;
				}
				g_power.slp_typa_s5 = *s5_ptr;
				s5_ptr++;

				if (*s5_ptr == 0x0A) {
					s5_ptr++;
				}
				g_power.slp_typb_s5 = *s5_ptr;

				drivers_log("[POWER] S5 SLP_TYP: a=%u, "
				    "b=%u\n", g_power.slp_typa_s5,
				    g_power.slp_typb_s5);
				(void)pkg_len;
				return (0);
			}
		}
	}

	drivers_log("[POWER] _S5_ object not found in DSDT\n");
	return (-1);
}

int
power_init(void)
{
	acpi_fadt_t	*fadt;

	memset(&g_power, 0, sizeof(g_power));

	if (!acpi_is_initialized()) {
		drivers_log("[POWER] ACPI not available, "
		    "limited power management\n");
		g_power.initialized = 1;
		return (-1);
	}

	fadt = acpi_get_fadt();
	if (!fadt) {
		drivers_log("[POWER] FADT not found, "
		    "limited power management\n");
		g_power.initialized = 1;
		return (-1);
	}

	g_power.pm1a_control = (u16)fadt->pm1a_control_block;
	g_power.pm1b_control = (u16)fadt->pm1b_control_block;
	g_power.smi_command_port = fadt->smi_command_port;
	g_power.acpi_enable_value = fadt->acpi_enable;
	g_power.acpi_available = 1;

	if (fadt->flags & ACPI_FADT_RESET_REG_SUP) {
		g_power.reset_reg_available = 1;
		drivers_log("[POWER] ACPI reset register available "
		    "at 0x%x\n",
		    (u32)fadt->reset_reg.address);
	}

	parse_s5_from_dsdt(fadt);

	g_power.initialized = 1;

	drivers_log("[POWER] initialized: PM1a=0x%x PM1b=0x%x "
	    "SLP_TYP_S5=%u/%u\n",
	    g_power.pm1a_control, g_power.pm1b_control,
	    g_power.slp_typa_s5, g_power.slp_typb_s5);

	return (0);
}

void
power_shutdown(void)
{
	u16	slp_typa;

	drivers_log("[POWER] shutting down...\n");

	__asm__ volatile("cli");

	if (g_power.acpi_available && g_power.pm1a_control) {
		volatile int	i;

		slp_typa = (g_power.slp_typa_s5 << 10) | PM1_SLP_EN;
		drivers_log("[POWER] writing 0x%x to PM1a control "
		    "(0x%x)\n", slp_typa, g_power.pm1a_control);
		outw(g_power.pm1a_control, slp_typa);

		if (g_power.pm1b_control) {
			u16	slp_typb;

			slp_typb = (g_power.slp_typb_s5 << 10) |
			    PM1_SLP_EN;
			outw(g_power.pm1b_control, slp_typb);
		}

		for (i = 0; i < 10000000; i++) {
			__asm__ volatile("pause");
		}
	}

	panic("[POWER] ACPI S5 shutdown failed, system cannot "
	    "power off\n");
}

void
power_reboot(void)
{
	volatile int	i;
	u8		good;

	drivers_log("[POWER] rebooting...\n");

	__asm__ volatile("cli");

	if (g_power.acpi_available && g_power.reset_reg_available) {
		acpi_fadt_t	*fadt;

		fadt = acpi_get_fadt();
		if (fadt) {
			if (fadt->reset_reg.address_space ==
			    ACPI_GAS_SYSTEM_IO) {
				drivers_log("[POWER] ACPI reset via "
				    "I/O port 0x%x = 0x%x\n",
				    (u32)fadt->reset_reg.address,
				    fadt->reset_value);
				outb((u16)fadt->reset_reg.address,
				    fadt->reset_value);
			} else if (fadt->reset_reg.address_space ==
			    ACPI_GAS_SYSTEM_MEMORY) {
				volatile u8	*reg;

				drivers_log("[POWER] ACPI reset via "
				    "MMIO 0x%x = 0x%x\n",
				    (u32)fadt->reset_reg.address,
				    fadt->reset_value);
				reg = (volatile u8 *)
				    fadt->reset_reg.address;
				*reg = fadt->reset_value;
			}

			for (i = 0; i < 10000000; i++) {
				__asm__ volatile("pause");
			}
		}
	}

	drivers_log("[POWER] trying PS/2 keyboard controller "
	    "reset\n");

	good = 0x02;
	while (good & 0x02) {
		good = inb(KB_CTRL_PORT);
	}
	outb(KB_CTRL_PORT, KB_RESET_CMD);

	for (i = 0; i < 10000000; i++) {
		__asm__ volatile("pause");
	}

	drivers_log("[POWER] triple-faulting...\n");
	struct {
		u16	limit;
		u64	base;
	} __attribute__((packed)) null_idt = {0, 0};
	__asm__ volatile("lidt %0" : : "m"(null_idt));
	__asm__ volatile("int $0x03");

	while (1) {
		__asm__ volatile("hlt");
	}
}

int
power_acpi_enable(void)
{
	u16	pm1a;
	int	i;

	if (!g_power.acpi_available || !g_power.smi_command_port) {
		drivers_log("[POWER] SMI command port not "
		    "available\n");
		return (-1);
	}

	pm1a = inw(g_power.pm1a_control);
	if (pm1a & 1) {
		drivers_log("[POWER] ACPI already enabled\n");
		return (0);
	}

	drivers_log("[POWER] enabling ACPI via SMI cmd port "
	    "0x%x, value 0x%x\n",
	    g_power.smi_command_port, g_power.acpi_enable_value);
	outb((u16)g_power.smi_command_port, g_power.acpi_enable_value);

	for (i = 0; i < 3000; i++) {
		volatile int	j;

		pm1a = inw(g_power.pm1a_control);
		if (pm1a & 1) {
			drivers_log("[POWER] ACPI mode enabled "
			    "successfully\n");
			return (0);
		}
		for (j = 0; j < 10000; j++) {
			__asm__ volatile("pause");
		}
	}

	panic("[POWER] failed to enable ACPI mode (timeout)\n");
	return (-1);
}

power_info_t
power_get_info(void)
{
	return (g_power);
}

int
power_is_initialized(void)
{
	return (g_power.initialized);
}

static void
power_core_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "power_core", 0) == NULL) {
		device_add_child(parent, "power_core", 0);
	}
}

static int
power_core_attach(device_t dev)
{
	power_info_t	info;

	(void)dev;
	(void)power_init();
	info = power_get_info();
	if (info.acpi_available) {
		(void)power_acpi_enable();
	}
	return (0);
}

static devclass_t power_core_devclass = {
	.name		= "power",
	.maxunit	= 1,
};

static driver_t power_core_driver = {
	.name		= "power_core",
	.identify	= power_core_identify,
	.probe		= NULL,
	.attach		= power_core_attach,
};

PLATFORM_DRIVER_MODULE(power_core, power_core_driver, power_core_devclass,
    NEWBUS_PASS_LATE, NEWBUS_ORDER_FIRST);
