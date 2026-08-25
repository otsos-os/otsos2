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
$define %type aml_object_type_t as enum with AML object types
$define %type aml_object_t as reference counted AML data object
$define %type aml_node_t as AML namespace node
$define %type aml_irq_t as decoded interrupt resource descriptor

$define %func aml_init as function with args void
$define %func aml_is_initialized as function with args void
$define %func aml_root as function with args void
$define %func aml_resolve as function with args aml_node_t *, const char *
$define %func aml_node_child as function with args aml_node_t *, const char *
$define %func aml_node_path as function with args aml_node_t *, char *, u32
$define %func aml_object_ref as procedure with args aml_object_t *
$define %func aml_object_unref as procedure with args aml_object_t *
$define %func aml_integer_create as function with args u64
$define %func aml_string_create as function with args const char *
$define %func aml_buffer_create as function with args u32
$define %func aml_package_create as function with args u32
$define %func aml_object_as_integer as function with args aml_object_t *, u64 *
$define %func aml_evaluate as function with args aml_node_t *, aml_object_t **, u32, aml_object_t **
$define %func aml_evaluate_path as function with args aml_node_t *, const char *, aml_object_t **
$define %func aml_evaluate_integer as function with args aml_node_t *, const char *, u64 *
$define %func aml_node_status as function with args aml_node_t *
$define %func aml_node_hid as function with args aml_node_t *, char *, u32
$define %func aml_walk as function with args aml_node_t *, int (*)(aml_node_t *, void *), void *
$define %type aml_resource_t as decoded ACPI resource descriptor
$define %func aml_resource_walk as function with args aml_object_t *, int (*)(const aml_resource_t *, void *), void *
$define %func aml_resource_irqs as function with args aml_object_t *, aml_irq_t *, u32
$define %func aml_dump_namespace as procedure with args void

*/

/* !SPACE!

$space %export aml_init, aml_is_initialized, aml_root
$space %export aml_resolve, aml_node_child, aml_node_path, aml_walk
$space %export aml_object_ref, aml_object_unref
$space %export aml_integer_create, aml_string_create
$space %export aml_buffer_create, aml_package_create
$space %export aml_object_as_integer
$space %export aml_evaluate, aml_evaluate_path, aml_evaluate_integer
$space %export aml_node_status, aml_node_hid
$space %export aml_resource_walk, aml_resource_irqs
$space %export aml_dump_namespace

*/

#ifndef ACPI_AML_H
#define ACPI_AML_H

#include <mlibc/mlibc.h>

#define	AML_NAME_LENGTH		4
#define	AML_MAX_ARGS		7
#define	AML_MAX_LOCALS		8
#define	AML_MAX_PATH		260
#define	AML_MAX_NESTING		96

typedef enum {
	AML_TYPE_UNINITIALIZED	= 0,
	AML_TYPE_INTEGER	= 1,
	AML_TYPE_STRING		= 2,
	AML_TYPE_BUFFER		= 3,
	AML_TYPE_PACKAGE	= 4,
	AML_TYPE_FIELD_UNIT	= 5,
	AML_TYPE_DEVICE		= 6,
	AML_TYPE_EVENT		= 7,
	AML_TYPE_METHOD		= 8,
	AML_TYPE_MUTEX		= 9,
	AML_TYPE_REGION		= 10,
	AML_TYPE_POWER		= 11,
	AML_TYPE_PROCESSOR	= 12,
	AML_TYPE_THERMAL	= 13,
	AML_TYPE_BUFFER_FIELD	= 14,
	AML_TYPE_DEBUG		= 16,
	AML_TYPE_REFERENCE	= 20,
	AML_TYPE_SCOPE		= 21
} aml_object_type_t;

#define	AML_REGION_SYSTEM_MEMORY	0x00
#define	AML_REGION_SYSTEM_IO		0x01
#define	AML_REGION_PCI_CONFIG		0x02
#define	AML_REGION_EMBEDDED_CONTROL	0x03
#define	AML_REGION_SMBUS		0x04
#define	AML_REGION_CMOS			0x05
#define	AML_REGION_PCI_BAR_TARGET	0x06
#define	AML_REGION_IPMI			0x07

#define	AML_FIELD_ACCESS_ANY		0
#define	AML_FIELD_ACCESS_BYTE		1
#define	AML_FIELD_ACCESS_WORD		2
#define	AML_FIELD_ACCESS_DWORD		3
#define	AML_FIELD_ACCESS_QWORD		4
#define	AML_FIELD_ACCESS_BUFFER		5

#define	AML_UPDATE_PRESERVE		0
#define	AML_UPDATE_WRITE_AS_ONES	1
#define	AML_UPDATE_WRITE_AS_ZEROS	2

#define	AML_STA_PRESENT			0x01
#define	AML_STA_ENABLED			0x02
#define	AML_STA_SHOWN			0x04
#define	AML_STA_FUNCTIONING		0x08
#define	AML_STA_BATTERY			0x10

typedef struct aml_object	aml_object_t;
typedef struct aml_node		aml_node_t;

typedef enum {
	AML_REF_NAMED		= 0,
	AML_REF_INDEX_PACKAGE	= 1,
	AML_REF_INDEX_BUFFER	= 2,
	AML_REF_INDEX_STRING	= 3,
	AML_REF_DEBUG		= 4,
	AML_REF_LOCAL		= 5,
	AML_REF_ARG		= 6
} aml_ref_kind_t;

struct aml_object {
	aml_object_type_t	type;
	u32			refcount;
	union {
		u64	integer;
		struct {
			char	*data;
			u32	length;
		} string;
		struct {
			u8	*data;
			u32	length;
		} buffer;
		struct {
			aml_object_t	**elements;
			u32		count;
		} package;
		struct {
			u8	space;
			u64	offset;
			u64	length;
			void	*mapped;
			u64	mapped_length;
			u16	pci_segment;
			u8	pci_bus;
			u8	pci_slot;
			u8	pci_function;
			u8	pci_resolved;
		} region;
		struct {
			aml_node_t	*region;
			u32		bit_offset;
			u32		bit_length;
			u8		access;
			u8		update;
			u8		lock;
			aml_node_t	*bank;
			u64		bank_value;
			aml_node_t	*index;
			aml_node_t	*data;
			u8		is_index;
			u8		is_bank;
		} field;
		struct {
			aml_object_t	*buffer;
			u32		bit_offset;
			u32		bit_length;
		} buffer_field;
		struct {
			const u8	*aml;
			u32		length;
			u8		arg_count;
			u8		serialized;
			u8		sync_level;
			aml_node_t	*scope;
		} method;
		struct {
			u32	sync_level;
			u32	owner_depth;
		} mutex;
		struct {
			u32	signals;
		} event;
		struct {
			u8	id;
			u32	block_address;
			u8	block_length;
		} processor;
		struct {
			u8	system_level;
			u16	resource_order;
		} power;
		struct {
			aml_ref_kind_t	kind;
			aml_node_t	*node;
			aml_object_t	*container;
			u32		index;
		} reference;
	} u;
};

struct aml_node {
	char		name[AML_NAME_LENGTH + 1];
	aml_node_t	*parent;
	aml_node_t	*child;
	aml_node_t	*sibling;
	aml_object_t	*object;
	u8		is_alias;
	aml_node_t	*alias_target;
};

typedef struct {
	u32	gsi;
	u8	triggering;
	u8	polarity;
	u8	sharing;
	u8	wake_capable;
	u8	is_extended;
} aml_irq_t;

#define	AML_TRIGGER_LEVEL	0
#define	AML_TRIGGER_EDGE	1
#define	AML_POLARITY_HIGH	0
#define	AML_POLARITY_LOW	1
#define	AML_SHARING_EXCLUSIVE	0
#define	AML_SHARING_SHARED	1

#define	AML_RES_UNKNOWN		0
#define	AML_RES_IRQ		1
#define	AML_RES_DMA		2
#define	AML_RES_START_DEPENDENT	3
#define	AML_RES_END_DEPENDENT	4
#define	AML_RES_IO		5
#define	AML_RES_FIXED_IO	6
#define	AML_RES_FIXED_DMA	7
#define	AML_RES_VENDOR		8
#define	AML_RES_MEMORY		9
#define	AML_RES_GENERIC_REG	10
#define	AML_RES_ADDRESS		11
#define	AML_RES_EXT_IRQ		12
#define	AML_RES_GPIO		13
#define	AML_RES_PIN_FUNCTION	14
#define	AML_RES_SERIAL_BUS	15
#define	AML_RES_PIN_CONFIG	16
#define	AML_RES_PIN_GROUP	17
#define	AML_RES_CLOCK_INPUT	18
#define	AML_RES_END		19

#define	AML_RES_TYPE_MEMORY	0
#define	AML_RES_TYPE_IO		1
#define	AML_RES_TYPE_BUS	2

typedef struct {
	u32		type;
	u8		tag;
	u8		large;
	u8		resource_type;
	u8		type_flags;
	u8		producer;
	u8		decode_subtractive;
	u8		decode_16;
	u8		min_fixed;
	u8		max_fixed;
	u8		write_protect;
	u8		caching;
	u8		range_type;
	u8		translation;
	u8		sparse;
	u8		priority;
	aml_irq_t	irq;
	u32		irq_mask;
	const u8	*gsi_table;
	u8		gsi_count;
	u8		dma_mask;
	u8		dma_transfer;
	u8		dma_type;
	u8		bus_master;
	u32		dma_request;
	u32		dma_channel;
	u64		granularity;
	u64		minimum;
	u64		maximum;
	u64		translation_offset;
	u64		length;
	u64		attribute;
	u32		alignment;
	u8		space_id;
	u8		bit_width;
	u8		bit_offset;
	u8		access_size;
	u8		source_index;
	const char	*source;
	const u8	*vendor_data;
	u32		vendor_length;
} aml_resource_t;

int		aml_init(void);
int		aml_is_initialized(void);
aml_node_t	*aml_root(void);
aml_node_t	*aml_resolve(aml_node_t *scope, const char *path);
aml_node_t	*aml_node_child(aml_node_t *parent, const char *name);
int		aml_node_path(aml_node_t *node, char *out, u32 size);
int		aml_walk(aml_node_t *start, int (*fn)(aml_node_t *, void *),
		    void *ctx);

void		aml_object_ref(aml_object_t *object);
void		aml_object_unref(aml_object_t *object);
aml_object_t	*aml_integer_create(u64 value);
aml_object_t	*aml_string_create(const char *text);
aml_object_t	*aml_buffer_create(u32 length);
aml_object_t	*aml_package_create(u32 count);
int		aml_object_as_integer(aml_object_t *object, u64 *value);

int		aml_evaluate(aml_node_t *node, aml_object_t **args,
		    u32 arg_count, aml_object_t **result);
int		aml_evaluate_path(aml_node_t *scope, const char *path,
		    aml_object_t **result);
int		aml_evaluate_integer(aml_node_t *scope, const char *path,
		    u64 *value);
int		aml_node_status(aml_node_t *node);
int		aml_node_hid(aml_node_t *node, char *out, u32 size);

int		aml_resource_walk(aml_object_t *buffer,
		    int (*fn)(const aml_resource_t *, void *), void *ctx);
int		aml_resource_irqs(aml_object_t *buffer, aml_irq_t *out,
		    u32 max);
void		aml_dump_namespace(void);

#endif

