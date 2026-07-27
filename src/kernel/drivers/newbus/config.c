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
$define %type newbus_module_t as linker-set driver declaration
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed

$define %func newbus_config_policy_enabled as function with args void
$define %func newbus_config_bool as function with args const char *, const char *, int
$define %func newbus_config_build_key as function with args char *, u32, const char *, const char *
$define %func newbus_config_driver_name as function with args const newbus_module_t *
$define %func newbus_config_driver_enabled as function with args const newbus_module_t *
$define %func newbus_config_device_enabled as function with args device_t
$define %func newbus_config_bus_enabled as function with args const char *
$define %func newbus_config_probe_allowed as function with args device_t, const newbus_module_t *
$define %func newbus_config_driver_get_bool as function with args const char *, const char *, int
$define %func newbus_config_driver_get_u32 as function with args const char *, const char *, u32
$define %func newbus_config_driver_get_string as function with args const char *, const char *, char *, u32, const char *

*/

/* !SPACE!

$space %internal newbus_config_policy_enabled, newbus_config_bool
$space %internal newbus_config_build_key, newbus_config_driver_name
$space %export newbus_config_driver_enabled
$space %export newbus_config_device_enabled, newbus_config_bus_enabled
$space %export newbus_config_probe_allowed
$space %export newbus_config_driver_get_bool
$space %export newbus_config_driver_get_u32
$space %export newbus_config_driver_get_string

*/

#include <kernel/cm/cm.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	NEWBUS_CONFIG_HIVE	"SYSTEM"
#define	NEWBUS_CONFIG_KEY_MAX	128

static int
newbus_config_policy_enabled(void)
{
	if (!cm_is_initialized()) {
		return (0);
	}
	return (cm_get_bool_default(NEWBUS_CONFIG_HIVE, "Newbus",
	    "PolicyEnabled", 1));
}

static int
newbus_config_bool(const char *key, const char *value, int default_val)
{
	if (!cm_is_initialized() || !newbus_config_policy_enabled()) {
		return (default_val ? 1 : 0);
	}
	return (cm_get_bool_default(NEWBUS_CONFIG_HIVE, key, value,
	    default_val));
}

static int
newbus_config_build_key(char *out, u32 out_size, const char *prefix,
    const char *name)
{
	int	ret;

	if (out == NULL || out_size == 0 || prefix == NULL ||
	    name == NULL || name[0] == '\0') {
		return (-1);
	}
	ret = snprintf(out, out_size, "%s.%s", prefix, name);
	if (ret < 0 || (u32)ret >= out_size) {
		return (-1);
	}
	return (0);
}

static const char *
newbus_config_driver_name(const newbus_module_t *module)
{
	if (module == NULL) {
		return (NULL);
	}
	if (module->name != NULL && module->name[0] != '\0') {
		return (module->name);
	}
	if (module->driver != NULL && module->driver->name != NULL &&
	    module->driver->name[0] != '\0') {
		return (module->driver->name);
	}
	return (NULL);
}

int
newbus_config_driver_enabled(const newbus_module_t *module)
{
	char		key[NEWBUS_CONFIG_KEY_MAX];
	const char	*name;
	int		default_enabled;

	name = newbus_config_driver_name(module);
	if (name == NULL) {
		return (1);
	}
	default_enabled = newbus_config_bool("Newbus.Drivers",
	    "DefaultEnabled", 1);
	if (newbus_config_build_key(key, sizeof(key), "Newbus.Drivers",
	    name) != 0) {
		return (default_enabled);
	}
	if (!newbus_config_bool(key, "Enabled", default_enabled)) {
		return (0);
	}
	if (module->driver == NULL || module->driver->name == NULL ||
	    strcmp(module->driver->name, name) == 0) {
		return (1);
	}
	if (newbus_config_build_key(key, sizeof(key), "Newbus.Drivers",
	    module->driver->name) != 0) {
		return (1);
	}
	return (newbus_config_bool(key, "Enabled", 1));
}

int
newbus_config_device_enabled(device_t dev)
{
	char		key[NEWBUS_CONFIG_KEY_MAX];
	const char	*name;
	const char	*nameunit;
	int		default_enabled;

	if (dev == NULL) {
		return (1);
	}
	default_enabled = newbus_config_bool("Newbus.Devices",
	    "DefaultEnabled", 1);
	nameunit = device_get_nameunit(dev);
	if (nameunit != NULL &&
	    newbus_config_build_key(key, sizeof(key), "Newbus.Devices",
	    nameunit) == 0 &&
	    !newbus_config_bool(key, "Enabled", default_enabled)) {
		return (0);
	}
	name = device_get_name(dev);
	if (name != NULL &&
	    newbus_config_build_key(key, sizeof(key), "Newbus.Devices",
	    name) == 0 &&
	    !newbus_config_bool(key, "Enabled", default_enabled)) {
		return (0);
	}
	return (1);
}

int
newbus_config_bus_enabled(const char *bus_name)
{
	char	key[NEWBUS_CONFIG_KEY_MAX];
	int	default_enabled;

	if (bus_name == NULL || bus_name[0] == '\0') {
		return (1);
	}
	default_enabled = newbus_config_bool("Newbus.Buses",
	    "DefaultEnabled", 1);
	if (newbus_config_build_key(key, sizeof(key), "Newbus.Buses",
	    bus_name) != 0) {
		return (default_enabled);
	}
	return (newbus_config_bool(key, "Enabled", default_enabled));
}

int
newbus_config_probe_allowed(device_t dev, const newbus_module_t *module)
{
	if (!newbus_config_device_enabled(dev)) {
		return (0);
	}
	if (!newbus_config_driver_enabled(module)) {
		return (0);
	}
	if (module != NULL && !newbus_config_bus_enabled(module->bus_name)) {
		return (0);
	}
	return (1);
}

int
newbus_config_driver_get_bool(const char *driver_name, const char *value,
    int default_val)
{
	char	key[NEWBUS_CONFIG_KEY_MAX];

	if (newbus_config_build_key(key, sizeof(key),
	    "Newbus.Drivers", driver_name) != 0) {
		return (default_val ? 1 : 0);
	}
	return (newbus_config_bool(key, value, default_val));
}

u32
newbus_config_driver_get_u32(const char *driver_name, const char *value,
    u32 default_val)
{
	char	key[NEWBUS_CONFIG_KEY_MAX];

	if (!cm_is_initialized() || !newbus_config_policy_enabled() ||
	    newbus_config_build_key(key, sizeof(key), "Newbus.Drivers",
	    driver_name) != 0) {
		return (default_val);
	}
	return (cm_get_u32_default(NEWBUS_CONFIG_HIVE, key, value,
	    default_val));
}

int
newbus_config_driver_get_string(const char *driver_name, const char *value,
    char *out, u32 out_size, const char *default_val)
{
	char	key[NEWBUS_CONFIG_KEY_MAX];

	if (out == NULL || out_size == 0) {
		return (-1);
	}
	if (!cm_is_initialized() || !newbus_config_policy_enabled() ||
	    newbus_config_build_key(key, sizeof(key), "Newbus.Drivers",
	    driver_name) != 0) {
		strncpy(out, default_val == NULL ? "" : default_val,
		    out_size - 1);
		out[out_size - 1] = '\0';
		return (0);
	}
	return (cm_get_string_default(NEWBUS_CONFIG_HIVE, key, value,
	    out, out_size, default_val));
}
