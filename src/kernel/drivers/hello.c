/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type driver_t as newbus driver descriptor
$define %type devclass_t as newbus device class descriptor
$define %type newbus_module_t as dynamic newbus module descriptor
$define %type newbus_interface_t as named driver I/O interface table
$define %func kofo_module_init as function with args void
$define %func kofo_module_exit as function with args void
$define %func hello_input_read as function with args device_t, void *, u64, u64

*/

/* !SPACE!

$space %internal hello_identify, hello_probe, hello_attach, hello_detach
$space %internal hello_input_read
$space %export kofo_module_init, kofo_module_exit

*/

#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

static int	hello_registered;

static int
hello_input_read(device_t dev, void *buf, u64 count, u64 offset)
{
	static const char	hello_msg[] = "Hello from driver!";
	size_t			len;

	(void)dev;
	len = sizeof(hello_msg) - 1;
	if (offset >= len) {
		return (0);
	}
	if (count > len - offset) {
		count = len - offset;
	}
	memcpy(buf, hello_msg + offset, (unsigned long)count);
	return ((int)count);
}

static const newbus_interface_t hello_input_interface = {
	.name		= "input",
	.read		= hello_input_read,
};

static void
hello_identify(driver_t *driver, device_t parent)
{
	(void)driver;

	if (device_find_child(parent, "hello", 0) == NULL) {
		device_add_child(parent, "hello", 0);
	}
}

static int
hello_probe(device_t dev)
{
	if (strcmp(device_get_name(dev), "hello") == 0) {
		return (10);
	}
	return (-1);
}

static int
hello_attach(device_t dev)
{
	if (newbus_interface_register(dev, &hello_input_interface) != 0) {
		drivers_log("[hello-driver] interface register failed\n");
		return (-1);
	}
	drivers_log("[hello-driver] attached %s\n", device_get_nameunit(dev));
	return (0);
}

static int
hello_detach(device_t dev)
{
	if (newbus_interface_unregister(dev, &hello_input_interface) != 0) {
		drivers_log("[hello-driver] interface unregister failed\n");
		return (-1);
	}
	drivers_log("[hello-driver] detached %s\n", device_get_nameunit(dev));
	return (0);
}

static devclass_t hello_devclass = {
	.name		= "hello",
	.maxunit	= 1,
};

static driver_t hello_driver = {
	.name		= "hello",
	.identify	= hello_identify,
	.probe		= hello_probe,
	.attach		= hello_attach,
	.detach		= hello_detach,
};

static const newbus_module_t hello_module = {
	.name		= "hello-driver",
	.bus_name	= "pseudo",
	.driver		= &hello_driver,
	.devclass	= &hello_devclass,
	.pass		= NEWBUS_PASS_LATE,
	.order		= NEWBUS_ORDER_LAST,
};

int
kofo_module_init(void)
{
	if (!hello_registered) {
		if (newbus_driver_add_module(&hello_module) != 0) {
			return (-1);
		}
		hello_registered = 1;
		newbus_reprobe();
	}
	return (0);
}

int
kofo_module_exit(void)
{
	if (hello_registered) {
		if (newbus_driver_remove_module(&hello_module) != 0) {
			return (-1);
		}
		hello_registered = 0;
	}
	return (0);
}
