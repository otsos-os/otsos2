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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type aml_node_t as AML namespace node
$define %type aml_object_t as reference counted AML data object
$define %type aml_irq_t as decoded interrupt resource descriptor
$define %type acpi_prt_bridge_t as root bridge search context
$define %type acpi_prt_pick_t as link resource selection context
$define %type device_t as newbus device handle
$define %type pci_device_t as PCI function descriptor

$define %func acpi_prt_route as function with args u16, u8, u8, u8, u32 *, u32 *
$define %func acpi_prt_supported as function with args void
$define %func acpi_prt_rewire as function with args void

*/

/* !SPACE!

$space %internal acpi_prt_bridge_match, acpi_prt_root_bridge
$space %internal acpi_prt_integer_child, acpi_prt_parent_bridge
$space %internal acpi_prt_link_pick, acpi_prt_link_template
$space %internal acpi_prt_link_program, acpi_prt_link_irq
$space %internal acpi_prt_entry_match, acpi_prt_lookup
$space %export acpi_prt_route, acpi_prt_supported, acpi_prt_rewire

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/acpi/aml.h>
#include <kernel/drivers/acpi/prt.h>
#include <kernel/pci/pci.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	ACPI_PRT_MAX_IRQS	16
#define	ACPI_PRT_MAX_DEPTH	8
#define	ACPI_PRT_ANY_DEVICE	0xFFFF
#define	ACPI_PRT_PIN_COUNT	4
#define	ACPI_PCI_SECONDARY_BUS	0x19

static const char	*acpi_prt_bridge_ids[] = {
	"PNP0A03",
	"PNP0A08"
};

typedef struct {
	u16		segment;
	u8		bus;
	aml_node_t	*found;
} acpi_prt_bridge_t;

typedef struct {
	u8		tag;
	u8		large;
	u8		flags;
	u8		valid;
	u32		gsi;
} acpi_prt_pick_t;

static int
acpi_prt_integer_child(aml_node_t *node, const char *name, u64 *value)
{
	aml_object_t	*result;
	aml_node_t	*child;
	int		status;

	child = aml_node_child(node, name);
	if (child == NULL) {
		return (-1);
	}
	result = NULL;
	if (aml_evaluate(child, NULL, 0, &result) != 0) {
		return (-1);
	}
	status = aml_object_as_integer(result, value);
	aml_object_unref(result);
	return ((status == 0) ? 0 : -1);
}

static int
acpi_prt_bridge_match(aml_node_t *node, void *ctx)
{
	acpi_prt_bridge_t	*search;
	char			hid[16];
	u64			value;
	u32			i;
	u8			bus;
	u16			segment;

	search = ctx;
	if (node->object == NULL || node->object->type != AML_TYPE_DEVICE) {
		return (0);
	}
	if (aml_node_hid(node, hid, sizeof(hid)) != 0) {
		return (0);
	}
	for (i = 0; i < sizeof(acpi_prt_bridge_ids) /
	    sizeof(acpi_prt_bridge_ids[0]); i++) {
		if (strcmp(hid, acpi_prt_bridge_ids[i]) == 0) {
			break;
		}
	}
	if (i == sizeof(acpi_prt_bridge_ids) /
	    sizeof(acpi_prt_bridge_ids[0])) {
		return (0);
	}
	bus = 0;
	if (acpi_prt_integer_child(node, "_BBN", &value) == 0) {
		bus = (u8)value;
	}
	segment = 0;
	if (acpi_prt_integer_child(node, "_SEG", &value) == 0) {
		segment = (u16)value;
	}
	if (bus != search->bus || segment != search->segment) {
		return (0);
	}
	if ((aml_node_status(node) & AML_STA_PRESENT) == 0) {
		return (0);
	}
	search->found = node;
	return (-1);
}

static aml_node_t *
acpi_prt_root_bridge(u16 segment, u8 bus)
{
	acpi_prt_bridge_t	search;

	search.segment = segment;
	search.bus = bus;
	search.found = NULL;
	(void)aml_walk(NULL, acpi_prt_bridge_match, &search);
	return (search.found);
}

static int
acpi_prt_parent_bridge(u8 bus, u8 *parent_bus, u8 *parent_slot)
{
	pci_device_t	*dev;
	int		count;
	int		i;
	u8		secondary;

	count = pci_device_count();
	for (i = 0; i < count; i++) {
		dev = pci_get_device(i);
		if (dev == NULL) {
			continue;
		}
		if (dev->class_code != PCI_CLASS_BRIDGE ||
		    dev->subclass != PCI_SUBCLASS_PCI_TO_PCI) {
			continue;
		}
		secondary = pci_cfg_read8(dev->bus, dev->slot, dev->function,
		    ACPI_PCI_SECONDARY_BUS);
		if (secondary != bus) {
			continue;
		}
		*parent_bus = dev->bus;
		*parent_slot = dev->slot;
		return (0);
	}
	return (-1);
}

static int
acpi_prt_link_pick(const aml_resource_t *resource, void *ctx)
{
	acpi_prt_pick_t	*pick;

	pick = ctx;
	if (resource->type == AML_RES_IRQ) {
		pick->tag = resource->tag;
		pick->large = 0;
		pick->flags = (u8)((resource->irq.triggering & 0x01) |
		    ((resource->irq.polarity & 0x01) << 3) |
		    ((resource->irq.sharing & 0x01) << 4) |
		    ((resource->irq.wake_capable & 0x01) << 5));
		pick->valid = 1;
		return (1);
	}
	if (resource->type == AML_RES_EXT_IRQ) {
		pick->tag = resource->tag;
		pick->large = 1;
		pick->flags = (u8)((resource->producer & 0x01) |
		    ((resource->irq.triggering & 0x01) << 1) |
		    ((resource->irq.polarity & 0x01) << 2) |
		    ((resource->irq.sharing & 0x01) << 3) |
		    ((resource->irq.wake_capable & 0x01) << 4));
		pick->valid = 1;
		return (1);
	}
	return (0);
}

static aml_object_t *
acpi_prt_link_template(const acpi_prt_pick_t *pick, u32 gsi)
{
	aml_object_t	*buffer;
	u8		*data;

	if (pick->large) {
		buffer = aml_buffer_create(11);
		if (buffer == NULL) {
			return (NULL);
		}
		data = buffer->u.buffer.data;
		data[0] = 0x89;
		data[1] = 0x06;
		data[2] = 0x00;
		data[3] = pick->flags;
		data[4] = 0x01;
		data[5] = (u8)(gsi & 0xFF);
		data[6] = (u8)((gsi >> 8) & 0xFF);
		data[7] = (u8)((gsi >> 16) & 0xFF);
		data[8] = (u8)((gsi >> 24) & 0xFF);
		data[9] = 0x79;
		data[10] = 0x00;
		return (buffer);
	}
	if (gsi >= 16) {
		return (NULL);
	}
	buffer = aml_buffer_create(6);
	if (buffer == NULL) {
		return (NULL);
	}
	data = buffer->u.buffer.data;
	data[0] = 0x23;
	data[1] = (u8)((1U << gsi) & 0xFF);
	data[2] = (u8)(((1U << gsi) >> 8) & 0xFF);
	data[3] = pick->flags;
	data[4] = 0x79;
	data[5] = 0x00;
	return (buffer);
}

static int
acpi_prt_link_program(aml_node_t *link, aml_object_t *possible, u32 gsi)
{
	acpi_prt_pick_t	pick;
	aml_object_t	*template;
	aml_object_t	*args[1];
	aml_object_t	*result;
	aml_node_t	*method;
	int		status;

	method = aml_node_child(link, "_SRS");
	if (method == NULL || method->object == NULL ||
	    method->object->type != AML_TYPE_METHOD) {
		return (-1);
	}
	memset(&pick, 0, sizeof(pick));
	if (aml_resource_walk(possible, acpi_prt_link_pick, &pick) < 0 ||
	    pick.valid == 0) {
		return (-1);
	}
	template = acpi_prt_link_template(&pick, gsi);
	if (template == NULL) {
		return (-1);
	}
	args[0] = template;
	result = NULL;
	status = aml_evaluate(method, args, 1, &result);
	aml_object_unref(result);
	aml_object_unref(template);
	return ((status == 0) ? 0 : -1);
}

static int
acpi_prt_link_irq(aml_node_t *link, u32 index, u32 *gsi, u32 *flags)
{
	aml_irq_t	irqs[ACPI_PRT_MAX_IRQS];
	aml_object_t	*resources;
	aml_node_t	*method;
	int		count;

	if ((aml_node_status(link) & AML_STA_PRESENT) == 0) {
		return (-1);
	}
	method = aml_node_child(link, "_CRS");
	if (method != NULL) {
		resources = NULL;
		if (aml_evaluate(method, NULL, 0, &resources) == 0) {
			count = aml_resource_irqs(resources, irqs,
			    ACPI_PRT_MAX_IRQS);
			if (count > 0 && index < (u32)count &&
			    irqs[index].gsi != 0) {
				*gsi = irqs[index].gsi;
				*flags = RF_SHAREABLE | RF_IRQ_GSI;
				if (irqs[index].triggering ==
				    AML_TRIGGER_LEVEL) {
					*flags |= RF_IRQ_LEVEL;
				}
				if (irqs[index].polarity ==
				    AML_POLARITY_LOW) {
					*flags |= RF_IRQ_ACTIVE_LOW;
				}
				aml_object_unref(resources);
				return (0);
			}
			aml_object_unref(resources);
		}
	}
	method = aml_node_child(link, "_PRS");
	if (method == NULL) {
		return (-1);
	}
	resources = NULL;
	if (aml_evaluate(method, NULL, 0, &resources) != 0) {
		return (-1);
	}
	count = aml_resource_irqs(resources, irqs, ACPI_PRT_MAX_IRQS);
	if (count <= 0 || index >= (u32)count || irqs[index].gsi == 0) {
		aml_object_unref(resources);
		return (-1);
	}
	if (acpi_prt_link_program(link, resources, irqs[index].gsi) != 0) {
		aml_object_unref(resources);
		return (-1);
	}
	*gsi = irqs[index].gsi;
	*flags = RF_SHAREABLE | RF_IRQ_GSI;
	if (irqs[index].triggering == AML_TRIGGER_LEVEL) {
		*flags |= RF_IRQ_LEVEL;
	}
	if (irqs[index].polarity == AML_POLARITY_LOW) {
		*flags |= RF_IRQ_ACTIVE_LOW;
	}
	aml_object_unref(resources);
	return (0);
}

static int
acpi_prt_entry_match(aml_object_t *entry, u8 slot, u8 pin, u32 *gsi,
    u32 *flags)
{
	aml_object_t	*source;
	aml_node_t	*link;
	u64		address;
	u64		entry_pin;
	u64		index;

	if (entry == NULL || entry->type != AML_TYPE_PACKAGE ||
	    entry->u.package.count < 4) {
		return (-1);
	}
	if (aml_object_as_integer(entry->u.package.elements[0],
	    &address) != 0 ||
	    aml_object_as_integer(entry->u.package.elements[1],
	    &entry_pin) != 0 ||
	    aml_object_as_integer(entry->u.package.elements[3],
	    &index) != 0) {
		return (-1);
	}
	if (entry_pin != pin) {
		return (-1);
	}
	if (((address >> 16) & 0xFFFF) != ACPI_PRT_ANY_DEVICE &&
	    ((address >> 16) & 0xFFFF) != slot) {
		return (-1);
	}
	source = entry->u.package.elements[2];
	if (source == NULL) {
		return (-1);
	}
	if (source->type == AML_TYPE_INTEGER && source->u.integer == 0) {
		*gsi = (u32)index;
		*flags = RF_SHAREABLE | RF_IRQ_GSI | RF_IRQ_LEVEL |
		    RF_IRQ_ACTIVE_LOW;
		return (0);
	}
	link = NULL;
	if (source->type == AML_TYPE_REFERENCE) {
		link = source->u.reference.node;
	} else if (source->type == AML_TYPE_STRING) {
		link = aml_resolve(NULL, source->u.string.data);
	} else if (source->type == AML_TYPE_INTEGER) {
		*gsi = (u32)index;
		*flags = RF_SHAREABLE | RF_IRQ_GSI | RF_IRQ_LEVEL |
		    RF_IRQ_ACTIVE_LOW;
		return (0);
	}
	if (link == NULL) {
		return (-1);
	}
	return (acpi_prt_link_irq(link, (u32)index, gsi, flags));
}

static int
acpi_prt_lookup(aml_node_t *bridge, u8 slot, u8 pin, u32 *gsi, u32 *flags)
{
	aml_object_t	*table;
	aml_node_t	*method;
	u32		i;
	int		status;

	method = aml_node_child(bridge, "_PRT");
	if (method == NULL) {
		return (-1);
	}
	table = NULL;
	if (aml_evaluate(method, NULL, 0, &table) != 0) {
		return (-1);
	}
	if (table->type != AML_TYPE_PACKAGE) {
		aml_object_unref(table);
		return (-1);
	}
	status = -1;
	for (i = 0; i < table->u.package.count; i++) {
		if (acpi_prt_entry_match(table->u.package.elements[i], slot,
		    pin, gsi, flags) == 0) {
			status = 0;
			break;
		}
	}
	aml_object_unref(table);
	return (status);
}

int
acpi_prt_route(u16 segment, u8 bus, u8 slot, u8 pin, u32 *gsi, u32 *flags)
{
	aml_node_t	*bridge;
	u32		depth;
	u8		current_bus;
	u8		current_slot;
	u8		current_pin;
	u8		parent_bus;
	u8		parent_slot;

	if (gsi == NULL || flags == NULL || pin < 1 ||
	    pin > ACPI_PRT_PIN_COUNT) {
		return (-1);
	}
	if (!aml_is_initialized()) {
		return (-1);
	}
	current_bus = bus;
	current_slot = slot;
	current_pin = pin;
	for (depth = 0; depth < ACPI_PRT_MAX_DEPTH; depth++) {
		bridge = acpi_prt_root_bridge(segment, current_bus);
		if (bridge != NULL) {
			return (acpi_prt_lookup(bridge, current_slot,
			    current_pin, gsi, flags));
		}
		if (acpi_prt_parent_bridge(current_bus, &parent_bus,
		    &parent_slot) != 0) {
			return (-1);
		}
		current_pin = (u8)(((current_pin - 1 + current_slot) %
		    ACPI_PRT_PIN_COUNT) + 1);
		current_bus = parent_bus;
		current_slot = parent_slot;
	}
	return (-1);
}

int
acpi_prt_supported(void)
{
	return (aml_is_initialized());
}

int
acpi_prt_rewire(void)
{
	pci_device_t	*pdev;
	device_t	dev;
	u32		gsi;
	u32		flags;
	int		count;
	int		routed;
	int		i;

	if (!aml_is_initialized()) {
		return (0);
	}
	routed = 0;
	count = newbus_device_count_get();
	for (i = 0; i < count; i++) {
		dev = newbus_device_get(i);
		if (dev == NULL) {
			continue;
		}
		if (strcmp(device_get_name(dev), "pcifn") != 0) {
			continue;
		}
		pdev = device_get_ivars(dev);
		if (pdev == NULL || pdev->irq_pin == 0) {
			continue;
		}
		if (acpi_prt_route(0, pdev->bus, pdev->slot, pdev->irq_pin,
		    &gsi, &flags) != 0) {
			continue;
		}
		if (bus_set_resource(dev, SYS_RES_IRQ, 0, gsi, 1,
		    flags) != 0) {
			continue;
		}
		routed++;
	}
	return (routed);
}
