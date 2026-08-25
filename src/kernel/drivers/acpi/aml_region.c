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
$define %type aml_object_t as reference counted AML data object
$define %type aml_node_t as AML namespace node

$define %func aml_region_resolve_pci as function with args aml_node_t *, aml_object_t *
$define %func aml_region_read as function with args aml_object_t *, u64, u32, u64 *
$define %func aml_region_write as function with args aml_object_t *, u64, u32, u64
$define %func aml_field_read as function with args aml_state_t *, aml_object_t *, aml_object_t **
$define %func aml_field_write as function with args aml_state_t *, aml_object_t *, aml_object_t *
$define %func aml_buffer_field_read as function with args aml_object_t *, aml_object_t **
$define %func aml_buffer_field_write as function with args aml_object_t *, aml_object_t *

*/

/* !SPACE!

$space %internal aml_inl, aml_outl, aml_region_map, aml_access_bits
$space %internal aml_region_mem_read, aml_region_mem_write
$space %internal aml_region_io_read, aml_region_io_write
$space %internal aml_region_pci_read, aml_region_pci_write
$space %internal aml_field_unit_read, aml_field_unit_write
$space %internal aml_field_prepare, aml_bits_extract, aml_bits_insert
$space %export aml_region_resolve_pci, aml_region_read, aml_region_write
$space %export aml_field_read, aml_field_write
$space %export aml_buffer_field_read, aml_buffer_field_write

*/

#include <kernel/drivers/acpi/amlint.h>
#include <kernel/mm/vm/pmap.h>
#include <kernel/mm/kmem.h>
#include <kernel/pci/pci.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	AML_REGION_MAP_LIMIT	(16U * 1024U * 1024U)

static inline u32
aml_inl(u16 port)
{
	u32	value;

	__asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
	return (value);
}

static inline void
aml_outl(u16 port, u32 value)
{
	__asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static int
aml_region_map(aml_object_t *region)
{
	if (region->u.region.mapped != NULL) {
		return (AML_OK);
	}
	if (region->u.region.length == 0 ||
	    region->u.region.length > AML_REGION_MAP_LIMIT) {
		return (AML_ERR_BOUNDS);
	}
	region->u.region.mapped = pmap_map_mmio(region->u.region.offset,
	    region->u.region.length);
	if (region->u.region.mapped == NULL) {
		return (AML_ERR);
	}
	region->u.region.mapped_length = region->u.region.length;
	return (AML_OK);
}

static int
aml_region_mem_read(aml_object_t *region, u64 offset, u32 width, u64 *value)
{
	volatile u8	*base;

	if (aml_region_map(region) != AML_OK) {
		return (AML_ERR);
	}
	if (offset + (width / 8) > region->u.region.mapped_length) {
		return (AML_ERR_BOUNDS);
	}
	base = (volatile u8 *)region->u.region.mapped + offset;
	switch (width) {
	case 8:
		*value = *base;
		return (AML_OK);
	case 16:
		*value = *(volatile u16 *)base;
		return (AML_OK);
	case 32:
		*value = *(volatile u32 *)base;
		return (AML_OK);
	case 64:
		*value = *(volatile u64 *)base;
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR);
}

static int
aml_region_mem_write(aml_object_t *region, u64 offset, u32 width, u64 value)
{
	volatile u8	*base;

	if (aml_region_map(region) != AML_OK) {
		return (AML_ERR);
	}
	if (offset + (width / 8) > region->u.region.mapped_length) {
		return (AML_ERR_BOUNDS);
	}
	base = (volatile u8 *)region->u.region.mapped + offset;
	switch (width) {
	case 8:
		*base = (u8)value;
		return (AML_OK);
	case 16:
		*(volatile u16 *)base = (u16)value;
		return (AML_OK);
	case 32:
		*(volatile u32 *)base = (u32)value;
		return (AML_OK);
	case 64:
		*(volatile u64 *)base = value;
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR);
}

static int
aml_region_io_read(aml_object_t *region, u64 offset, u32 width, u64 *value)
{
	u16	port;

	if (offset + (width / 8) > region->u.region.length) {
		return (AML_ERR_BOUNDS);
	}
	if (region->u.region.offset + offset > 0xFFFFU) {
		return (AML_ERR_BOUNDS);
	}
	port = (u16)(region->u.region.offset + offset);
	switch (width) {
	case 8:
		*value = inb(port);
		return (AML_OK);
	case 16:
		*value = inw(port);
		return (AML_OK);
	case 32:
		*value = aml_inl(port);
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR);
}

static int
aml_region_io_write(aml_object_t *region, u64 offset, u32 width, u64 value)
{
	u16	port;

	if (offset + (width / 8) > region->u.region.length) {
		return (AML_ERR_BOUNDS);
	}
	if (region->u.region.offset + offset > 0xFFFFU) {
		return (AML_ERR_BOUNDS);
	}
	port = (u16)(region->u.region.offset + offset);
	switch (width) {
	case 8:
		outb(port, (u8)value);
		return (AML_OK);
	case 16:
		outw(port, (u16)value);
		return (AML_OK);
	case 32:
		aml_outl(port, (u32)value);
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR);
}

static int
aml_region_pci_read(aml_object_t *region, u64 offset, u32 width, u64 *value)
{
	u64	target;

	if (!region->u.region.pci_resolved) {
		return (AML_ERR_NOT_FOUND);
	}
	target = region->u.region.offset + offset;
	if (target + (width / 8) > 0x1000U) {
		return (AML_ERR_BOUNDS);
	}
	switch (width) {
	case 8:
		*value = pci_cfg_read8(region->u.region.pci_bus,
		    region->u.region.pci_slot, region->u.region.pci_function,
		    (u8)target);
		return (AML_OK);
	case 16:
		*value = pci_cfg_read16(region->u.region.pci_bus,
		    region->u.region.pci_slot, region->u.region.pci_function,
		    (u8)target);
		return (AML_OK);
	case 32:
		*value = pci_cfg_read32(region->u.region.pci_bus,
		    region->u.region.pci_slot, region->u.region.pci_function,
		    (u8)target);
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR);
}

static int
aml_region_pci_write(aml_object_t *region, u64 offset, u32 width, u64 value)
{
	u64	target;

	if (!region->u.region.pci_resolved) {
		return (AML_ERR_NOT_FOUND);
	}
	target = region->u.region.offset + offset;
	if (target + (width / 8) > 0x1000U) {
		return (AML_ERR_BOUNDS);
	}
	switch (width) {
	case 8:
		pci_cfg_write8(region->u.region.pci_bus,
		    region->u.region.pci_slot, region->u.region.pci_function,
		    (u8)target, (u8)value);
		return (AML_OK);
	case 16:
		pci_cfg_write16(region->u.region.pci_bus,
		    region->u.region.pci_slot, region->u.region.pci_function,
		    (u8)target, (u16)value);
		return (AML_OK);
	case 32:
		pci_cfg_write32(region->u.region.pci_bus,
		    region->u.region.pci_slot, region->u.region.pci_function,
		    (u8)target, (u32)value);
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR);
}

#define	AML_PCI_DEPTH_MAX	8

int
aml_region_resolve_pci(aml_node_t *node, aml_object_t *region)
{
	char		hid[16];
	u64		chain[AML_PCI_DEPTH_MAX];
	aml_node_t	*scope;
	aml_node_t	*root_bridge;
	u64		value;
	u32		count;
	u32		i;
	u8		bus;
	u8		slot;
	u8		function;
	u8		secondary;

	if (node == NULL || region == NULL) {
		return (AML_ERR);
	}
	count = 0;
	root_bridge = NULL;
	for (scope = node->parent; scope != NULL; scope = scope->parent) {
		if (aml_node_hid(scope, hid, sizeof(hid)) == AML_OK &&
		    (strcmp(hid, "PNP0A03") == 0 ||
		    strcmp(hid, "PNP0A08") == 0)) {
			root_bridge = scope;
			break;
		}
		if (aml_evaluate_integer(scope, "_ADR", &value) == AML_OK) {
			if (count >= AML_PCI_DEPTH_MAX) {
				return (AML_ERR_DEPTH);
			}
			chain[count++] = value;
		}
	}
	if (root_bridge == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	bus = 0;
	if (aml_evaluate_integer(root_bridge, "_BBN", &value) == AML_OK) {
		bus = (u8)value;
	}
	if (count == 0) {
		return (AML_ERR_NOT_FOUND);
	}
	slot = 0;
	function = 0;
	for (i = count; i > 0; i--) {
		slot = (u8)((chain[i - 1] >> 16) & 0x1F);
		function = (u8)(chain[i - 1] & 0x07);
		if (i == 1) {
			break;
		}
		secondary = pci_cfg_read8(bus, slot, function, 0x19);
		if (secondary == 0 || secondary == 0xFF) {
			return (AML_ERR_NOT_FOUND);
		}
		bus = secondary;
	}
	region->u.region.pci_bus = bus;
	region->u.region.pci_slot = slot;
	region->u.region.pci_function = function;
	region->u.region.pci_resolved = 1;
	return (AML_OK);
}

#define	AML_CMOS_INDEX_PORT	0x70
#define	AML_CMOS_DATA_PORT	0x71

int
aml_region_read(aml_object_t *region, u64 offset, u32 width, u64 *value)
{
	if (region == NULL || value == NULL) {
		return (AML_ERR);
	}
	if (region->type != AML_TYPE_REGION) {
		return (AML_ERR_TYPE);
	}
	if (width != 8 && width != 16 && width != 32 && width != 64) {
		return (AML_ERR);
	}
	*value = 0;
	switch (region->u.region.space) {
	case AML_REGION_SYSTEM_MEMORY:
		return (aml_region_mem_read(region, offset, width, value));
	case AML_REGION_SYSTEM_IO:
		return (aml_region_io_read(region, offset, width, value));
	case AML_REGION_PCI_CONFIG:
		return (aml_region_pci_read(region, offset, width, value));
	case AML_REGION_CMOS:
		if (width != 8 || offset + 1 > region->u.region.length) {
			return (AML_ERR_BOUNDS);
		}
		outb(AML_CMOS_INDEX_PORT,
		    (u8)(region->u.region.offset + offset));
		*value = inb(AML_CMOS_DATA_PORT);
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR_UNSUPPORTED);
}

int
aml_region_write(aml_object_t *region, u64 offset, u32 width, u64 value)
{
	if (region == NULL) {
		return (AML_ERR);
	}
	if (region->type != AML_TYPE_REGION) {
		return (AML_ERR_TYPE);
	}
	if (width != 8 && width != 16 && width != 32 && width != 64) {
		return (AML_ERR);
	}
	switch (region->u.region.space) {
	case AML_REGION_SYSTEM_MEMORY:
		return (aml_region_mem_write(region, offset, width, value));
	case AML_REGION_SYSTEM_IO:
		return (aml_region_io_write(region, offset, width, value));
	case AML_REGION_PCI_CONFIG:
		return (aml_region_pci_write(region, offset, width, value));
	case AML_REGION_CMOS:
		if (width != 8 || offset + 1 > region->u.region.length) {
			return (AML_ERR_BOUNDS);
		}
		outb(AML_CMOS_INDEX_PORT,
		    (u8)(region->u.region.offset + offset));
		outb(AML_CMOS_DATA_PORT, (u8)value);
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR_UNSUPPORTED);
}

static u64
aml_bits_extract(const u8 *data, u32 length, u32 bit_offset, u32 bit_count)
{
	u64	value;
	u32	i;
	u32	index;

	value = 0;
	if (bit_count > 64) {
		bit_count = 64;
	}
	for (i = 0; i < bit_count; i++) {
		index = (bit_offset + i) / 8;
		if (index >= length) {
			break;
		}
		if ((data[index] & (u8)(1U << ((bit_offset + i) % 8))) != 0) {
			value |= 1ULL << i;
		}
	}
	return (value);
}

static void
aml_bits_insert(u8 *data, u32 length, u32 bit_offset, u32 bit_count, u64 value)
{
	u32	i;
	u32	index;

	if (bit_count > 64) {
		bit_count = 64;
	}
	for (i = 0; i < bit_count; i++) {
		index = (bit_offset + i) / 8;
		if (index >= length) {
			break;
		}
		if ((value & (1ULL << i)) != 0) {
			data[index] |= (u8)(1U << ((bit_offset + i) % 8));
		} else {
			data[index] &= (u8)~(1U << ((bit_offset + i) % 8));
		}
	}
}

static u32
aml_access_bits(u8 access)
{
	switch (access) {
	case AML_FIELD_ACCESS_WORD:
		return (16);
	case AML_FIELD_ACCESS_DWORD:
		return (32);
	case AML_FIELD_ACCESS_QWORD:
		return (64);
	default:
		break;
	}
	return (8);
}

static int	aml_field_read_raw(aml_state_t *state, aml_object_t *field,
		    u64 *value);
static int	aml_field_write_raw(aml_state_t *state, aml_object_t *field,
		    u64 value);

static int
aml_field_bank_select(aml_state_t *state, aml_object_t *field)
{
	aml_object_t	*value;
	int		status;

	if (!field->u.field.is_bank || field->u.field.bank == NULL) {
		return (AML_OK);
	}
	if (field->u.field.bank->object == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	value = aml_integer_create(field->u.field.bank_value);
	if (value == NULL) {
		return (AML_ERR_NOMEM);
	}
	status = aml_field_write(state, field->u.field.bank->object, value);
	aml_object_unref(value);
	return (status);
}

static int
aml_field_data_object(aml_object_t *field, aml_object_t **data)
{
	if (field->u.field.data == NULL ||
	    field->u.field.data->object == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	*data = field->u.field.data->object;
	return (AML_OK);
}

static int
aml_field_index_select(aml_state_t *state, aml_object_t *field, u64 offset)
{
	aml_object_t	*value;
	int		status;

	if (field->u.field.index == NULL ||
	    field->u.field.index->object == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	value = aml_integer_create(offset);
	if (value == NULL) {
		return (AML_ERR_NOMEM);
	}
	status = aml_field_write(state, field->u.field.index->object, value);
	aml_object_unref(value);
	return (status);
}

static int
aml_field_unit_read(aml_state_t *state, aml_object_t *field, u32 unit,
    u32 access, u64 *value)
{
	aml_object_t	*data;
	int		status;

	if (field->u.field.is_index) {
		status = aml_field_index_select(state, field,
		    (u64)unit * (access / 8));
		if (status != AML_OK) {
			return (status);
		}
		status = aml_field_data_object(field, &data);
		if (status != AML_OK) {
			return (status);
		}
		return (aml_field_read_raw(state, data, value));
	}
	if (field->u.field.region == NULL ||
	    field->u.field.region->object == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	return (aml_region_read(field->u.field.region->object,
	    (u64)unit * (access / 8), access, value));
}

static int
aml_field_unit_write(aml_state_t *state, aml_object_t *field, u32 unit,
    u32 access, u64 value)
{
	aml_object_t	*data;
	int		status;

	if (field->u.field.is_index) {
		status = aml_field_index_select(state, field,
		    (u64)unit * (access / 8));
		if (status != AML_OK) {
			return (status);
		}
		status = aml_field_data_object(field, &data);
		if (status != AML_OK) {
			return (status);
		}
		return (aml_field_write_raw(state, data, value));
	}
	if (field->u.field.region == NULL ||
	    field->u.field.region->object == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	return (aml_region_write(field->u.field.region->object,
	    (u64)unit * (access / 8), access, value));
}

#define	AML_FIELD_UNIT_LIMIT	4096

static u64
aml_mask_for(u32 bits)
{
	if (bits >= 64) {
		return (~0ULL);
	}
	return ((1ULL << bits) - 1ULL);
}

static int
aml_field_gather(aml_state_t *state, aml_object_t *field, u8 *out, u32 length)
{
	u64	unit_value;
	u32	access;
	u32	first;
	u32	last;
	u32	unit;
	u32	unit_start;
	u32	span_start;
	u32	span_end;
	u32	count;
	int	status;

	access = aml_access_bits(field->u.field.access);
	first = field->u.field.bit_offset / access;
	last = (field->u.field.bit_offset + field->u.field.bit_length - 1) /
	    access;
	if (last - first >= AML_FIELD_UNIT_LIMIT) {
		return (AML_ERR_BOUNDS);
	}
	status = aml_field_bank_select(state, field);
	if (status != AML_OK) {
		return (status);
	}
	for (unit = first; unit <= last; unit++) {
		status = aml_field_unit_read(state, field, unit, access,
		    &unit_value);
		if (status != AML_OK) {
			return (status);
		}
		unit_start = unit * access;
		span_start = (unit_start > field->u.field.bit_offset) ?
		    unit_start : field->u.field.bit_offset;
		span_end = unit_start + access;
		if (span_end > field->u.field.bit_offset +
		    field->u.field.bit_length) {
			span_end = field->u.field.bit_offset +
			    field->u.field.bit_length;
		}
		if (span_end <= span_start) {
			continue;
		}
		count = span_end - span_start;
		aml_bits_insert(out, length,
		    span_start - field->u.field.bit_offset, count,
		    (unit_value >> (span_start - unit_start)) &
		    aml_mask_for(count));
	}
	return (AML_OK);
}

static int
aml_field_scatter(aml_state_t *state, aml_object_t *field, const u8 *in,
    u32 length)
{
	u64	unit_value;
	u64	bits;
	u64	mask;
	u32	access;
	u32	first;
	u32	last;
	u32	unit;
	u32	unit_start;
	u32	span_start;
	u32	span_end;
	u32	count;
	int	status;
	int	partial;

	access = aml_access_bits(field->u.field.access);
	first = field->u.field.bit_offset / access;
	last = (field->u.field.bit_offset + field->u.field.bit_length - 1) /
	    access;
	if (last - first >= AML_FIELD_UNIT_LIMIT) {
		return (AML_ERR_BOUNDS);
	}
	status = aml_field_bank_select(state, field);
	if (status != AML_OK) {
		return (status);
	}
	for (unit = first; unit <= last; unit++) {
		unit_start = unit * access;
		span_start = (unit_start > field->u.field.bit_offset) ?
		    unit_start : field->u.field.bit_offset;
		span_end = unit_start + access;
		if (span_end > field->u.field.bit_offset +
		    field->u.field.bit_length) {
			span_end = field->u.field.bit_offset +
			    field->u.field.bit_length;
		}
		if (span_end <= span_start) {
			continue;
		}
		count = span_end - span_start;
		partial = (count != access);
		unit_value = 0;
		if (partial) {
			if (field->u.field.update == AML_UPDATE_WRITE_AS_ONES) {
				unit_value = aml_mask_for(access);
			} else if (field->u.field.update ==
			    AML_UPDATE_PRESERVE) {
				status = aml_field_unit_read(state, field, unit,
				    access, &unit_value);
				if (status != AML_OK) {
					return (status);
				}
			}
		}
		bits = aml_bits_extract(in, length,
		    span_start - field->u.field.bit_offset, count);
		mask = aml_mask_for(count) << (span_start - unit_start);
		unit_value &= ~mask;
		unit_value |= (bits << (span_start - unit_start)) & mask;
		status = aml_field_unit_write(state, field, unit, access,
		    unit_value);
		if (status != AML_OK) {
			return (status);
		}
	}
	return (AML_OK);
}

static int
aml_field_read_raw(aml_state_t *state, aml_object_t *field, u64 *value)
{
	u8	bytes[8];
	int	status;

	if (field->type != AML_TYPE_FIELD_UNIT) {
		return (AML_ERR_TYPE);
	}
	if (field->u.field.bit_length == 0 || field->u.field.bit_length > 64) {
		return (AML_ERR_BOUNDS);
	}
	memset(bytes, 0, sizeof(bytes));
	status = aml_field_gather(state, field, bytes, sizeof(bytes));
	if (status != AML_OK) {
		return (status);
	}
	*value = aml_bits_extract(bytes, sizeof(bytes), 0,
	    field->u.field.bit_length);
	return (AML_OK);
}

static int
aml_field_write_raw(aml_state_t *state, aml_object_t *field, u64 value)
{
	u8	bytes[8];

	if (field->type != AML_TYPE_FIELD_UNIT) {
		return (AML_ERR_TYPE);
	}
	if (field->u.field.bit_length == 0 || field->u.field.bit_length > 64) {
		return (AML_ERR_BOUNDS);
	}
	memset(bytes, 0, sizeof(bytes));
	aml_bits_insert(bytes, sizeof(bytes), 0, field->u.field.bit_length,
	    value);
	return (aml_field_scatter(state, field, bytes, sizeof(bytes)));
}

int
aml_field_read(aml_state_t *state, aml_object_t *field, aml_object_t **result)
{
	aml_object_t	*object;
	u64		value;
	u32		bytes;
	int		status;

	if (field == NULL || result == NULL) {
		return (AML_ERR);
	}
	if (field->type == AML_TYPE_BUFFER_FIELD) {
		return (aml_buffer_field_read(field, result));
	}
	if (field->type != AML_TYPE_FIELD_UNIT) {
		return (AML_ERR_TYPE);
	}
	if (field->u.field.bit_length == 0) {
		return (AML_ERR_BOUNDS);
	}
	if (field->u.field.lock) {
		aml_global_lock_acquire();
	}
	if (field->u.field.bit_length <= 64) {
		status = aml_field_read_raw(state, field, &value);
		if (status == AML_OK) {
			object = aml_integer_create(value);
			if (object == NULL) {
				status = AML_ERR_NOMEM;
			} else {
				*result = object;
			}
		}
	} else {
		bytes = (field->u.field.bit_length + 7) / 8;
		object = aml_buffer_create(bytes);
		if (object == NULL) {
			status = AML_ERR_NOMEM;
		} else {
			status = aml_field_gather(state, field,
			    object->u.buffer.data, bytes);
			if (status == AML_OK) {
				*result = object;
			} else {
				aml_object_unref(object);
			}
		}
	}
	if (field->u.field.lock) {
		aml_global_lock_release();
	}
	return (status);
}

int
aml_field_write(aml_state_t *state, aml_object_t *field, aml_object_t *value)
{
	aml_object_t	*buffer;
	u64		integer;
	u32		bytes;
	int		status;

	if (field == NULL || value == NULL) {
		return (AML_ERR);
	}
	if (field->type == AML_TYPE_BUFFER_FIELD) {
		return (aml_buffer_field_write(field, value));
	}
	if (field->type != AML_TYPE_FIELD_UNIT) {
		return (AML_ERR_TYPE);
	}
	if (field->u.field.bit_length == 0) {
		return (AML_ERR_BOUNDS);
	}
	if (field->u.field.lock) {
		aml_global_lock_acquire();
	}
	if (field->u.field.bit_length <= 64) {
		status = aml_object_as_integer(value, &integer);
		if (status == AML_OK) {
			status = aml_field_write_raw(state, field, integer);
		}
	} else {
		buffer = NULL;
		status = aml_object_to_buffer(value, &buffer);
		if (status == AML_OK) {
			bytes = (field->u.field.bit_length + 7) / 8;
			if (buffer->u.buffer.length < bytes) {
				bytes = buffer->u.buffer.length;
			}
			status = aml_field_scatter(state, field,
			    buffer->u.buffer.data, bytes);
			aml_object_unref(buffer);
		}
	}
	if (field->u.field.lock) {
		aml_global_lock_release();
	}
	return (status);
}

int
aml_buffer_field_read(aml_object_t *field, aml_object_t **result)
{
	aml_object_t	*buffer;
	aml_object_t	*object;
	u32		bytes;
	u32		i;

	if (field == NULL || result == NULL) {
		return (AML_ERR);
	}
	if (field->type != AML_TYPE_BUFFER_FIELD) {
		return (AML_ERR_TYPE);
	}
	buffer = field->u.buffer_field.buffer;
	if (buffer == NULL || buffer->type != AML_TYPE_BUFFER) {
		return (AML_ERR_TYPE);
	}
	if (field->u.buffer_field.bit_offset + field->u.buffer_field.bit_length >
	    buffer->u.buffer.length * 8) {
		return (AML_ERR_BOUNDS);
	}
	if (field->u.buffer_field.bit_length <= 64) {
		*result = aml_integer_create(aml_bits_extract(
		    buffer->u.buffer.data, buffer->u.buffer.length,
		    field->u.buffer_field.bit_offset,
		    field->u.buffer_field.bit_length));
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	}
	bytes = (field->u.buffer_field.bit_length + 7) / 8;
	object = aml_buffer_create(bytes);
	if (object == NULL) {
		return (AML_ERR_NOMEM);
	}
	for (i = 0; i < bytes; i++) {
		object->u.buffer.data[i] = (u8)aml_bits_extract(
		    buffer->u.buffer.data, buffer->u.buffer.length,
		    field->u.buffer_field.bit_offset + i * 8,
		    ((field->u.buffer_field.bit_length - i * 8) < 8) ?
		    (field->u.buffer_field.bit_length - i * 8) : 8);
	}
	*result = object;
	return (AML_OK);
}

int
aml_buffer_field_write(aml_object_t *field, aml_object_t *value)
{
	aml_object_t	*buffer;
	aml_object_t	*source;
	u64		integer;
	u32		bytes;
	u32		i;
	u32		remaining;
	int		status;

	if (field == NULL || value == NULL) {
		return (AML_ERR);
	}
	if (field->type != AML_TYPE_BUFFER_FIELD) {
		return (AML_ERR_TYPE);
	}
	buffer = field->u.buffer_field.buffer;
	if (buffer == NULL || buffer->type != AML_TYPE_BUFFER) {
		return (AML_ERR_TYPE);
	}
	if (field->u.buffer_field.bit_offset + field->u.buffer_field.bit_length >
	    buffer->u.buffer.length * 8) {
		return (AML_ERR_BOUNDS);
	}
	if (field->u.buffer_field.bit_length <= 64) {
		status = aml_object_as_integer(value, &integer);
		if (status != AML_OK) {
			return (status);
		}
		aml_bits_insert(buffer->u.buffer.data, buffer->u.buffer.length,
		    field->u.buffer_field.bit_offset,
		    field->u.buffer_field.bit_length, integer);
		return (AML_OK);
	}
	source = NULL;
	status = aml_object_to_buffer(value, &source);
	if (status != AML_OK) {
		return (status);
	}
	bytes = (field->u.buffer_field.bit_length + 7) / 8;
	for (i = 0; i < bytes; i++) {
		remaining = field->u.buffer_field.bit_length - i * 8;
		aml_bits_insert(buffer->u.buffer.data, buffer->u.buffer.length,
		    field->u.buffer_field.bit_offset + i * 8,
		    (remaining < 8) ? remaining : 8,
		    (i < source->u.buffer.length) ? source->u.buffer.data[i] : 0);
	}
	aml_object_unref(source);
	return (AML_OK);
}

