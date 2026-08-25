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
$define %type aml_resource_t as decoded ACPI resource descriptor
$define %type aml_irq_t as decoded interrupt resource descriptor

$define %func aml_resource_walk as function with args aml_object_t *, int (*)(const aml_resource_t *, void *), void *
$define %func aml_resource_irqs as function with args aml_object_t *, aml_irq_t *, u32

*/

/* !SPACE!

$space %internal aml_le16, aml_le32, aml_le64, aml_span
$space %internal aml_res_address, aml_res_small, aml_res_large
$space %internal aml_res_emit_irq, aml_res_collect
$space %export aml_resource_walk, aml_resource_irqs

*/

#include <kernel/drivers/acpi/amlint.h>
#include <mlibc/mlibc.h>

#define	AML_RES_LARGE_BIT	0x80
#define	AML_RES_SMALL_TAG(x)	(((x) >> 3) & 0x0F)
#define	AML_RES_SMALL_LEN(x)	((x) & 0x07)
#define	AML_RES_LARGE_TAG(x)	((x) & 0x7F)

#define	AML_SMALL_IRQ		0x04
#define	AML_SMALL_DMA		0x05
#define	AML_SMALL_START_DEP	0x06
#define	AML_SMALL_END_DEP	0x07
#define	AML_SMALL_IO		0x08
#define	AML_SMALL_FIXED_IO	0x09
#define	AML_SMALL_FIXED_DMA	0x0A
#define	AML_SMALL_VENDOR	0x0E
#define	AML_SMALL_END		0x0F

#define	AML_LARGE_MEMORY24	0x01
#define	AML_LARGE_GENERIC_REG	0x02
#define	AML_LARGE_VENDOR	0x04
#define	AML_LARGE_MEMORY32	0x05
#define	AML_LARGE_FIXED_MEM32	0x06
#define	AML_LARGE_DWORD_SPACE	0x07
#define	AML_LARGE_WORD_SPACE	0x08
#define	AML_LARGE_EXT_IRQ	0x09
#define	AML_LARGE_QWORD_SPACE	0x0A
#define	AML_LARGE_EXT_SPACE	0x0B
#define	AML_LARGE_GPIO		0x0C
#define	AML_LARGE_PIN_FUNCTION	0x0D
#define	AML_LARGE_SERIAL_BUS	0x0E
#define	AML_LARGE_PIN_CONFIG	0x0F
#define	AML_LARGE_PIN_GROUP	0x10
#define	AML_LARGE_CLOCK_INPUT	0x13

#define	AML_RES_MAX_DESCRIPTORS	512

static u16
aml_le16(const u8 *data)
{
	return ((u16)(data[0] | ((u16)data[1] << 8)));
}

static u32
aml_le32(const u8 *data)
{
	return ((u32)data[0] | ((u32)data[1] << 8) | ((u32)data[2] << 16) |
	    ((u32)data[3] << 24));
}

static u64
aml_le64(const u8 *data)
{
	return ((u64)aml_le32(data) | ((u64)aml_le32(data + 4) << 32));
}

static const char *
aml_span(const u8 *data, u32 length, u32 offset, u8 *index)
{
	if (offset >= length) {
		return (NULL);
	}
	*index = data[offset];
	if (offset + 1 >= length) {
		return (NULL);
	}
	return ((const char *)&data[offset + 1]);
}

static void
aml_res_address(aml_resource_t *out, const u8 *body, u32 length, u32 width)
{
	u32	offset;

	out->type = AML_RES_ADDRESS;
	out->resource_type = body[0];
	out->producer = (u8)((body[1] >> 0) & 0x01);
	out->decode_subtractive = (u8)((body[1] >> 1) & 0x01);
	out->min_fixed = (u8)((body[1] >> 2) & 0x01);
	out->max_fixed = (u8)((body[1] >> 3) & 0x01);
	out->type_flags = body[2];
	if (out->resource_type == AML_RES_TYPE_MEMORY) {
		out->write_protect = (u8)(body[2] & 0x01);
		out->caching = (u8)((body[2] >> 1) & 0x03);
		out->range_type = (u8)((body[2] >> 3) & 0x03);
		out->translation = (u8)((body[2] >> 5) & 0x01);
	} else if (out->resource_type == AML_RES_TYPE_IO) {
		out->range_type = (u8)(body[2] & 0x03);
		out->translation = (u8)((body[2] >> 4) & 0x01);
		out->sparse = (u8)((body[2] >> 5) & 0x01);
	}
	offset = 3;
	if (width == 8) {
		out->granularity = aml_le64(body + offset);
		out->minimum = aml_le64(body + offset + 8);
		out->maximum = aml_le64(body + offset + 16);
		out->translation_offset = aml_le64(body + offset + 24);
		out->length = aml_le64(body + offset + 32);
		offset += 40;
	} else if (width == 4) {
		out->granularity = aml_le32(body + offset);
		out->minimum = aml_le32(body + offset + 4);
		out->maximum = aml_le32(body + offset + 8);
		out->translation_offset = aml_le32(body + offset + 12);
		out->length = aml_le32(body + offset + 16);
		offset += 20;
	} else {
		out->granularity = aml_le16(body + offset);
		out->minimum = aml_le16(body + offset + 2);
		out->maximum = aml_le16(body + offset + 4);
		out->translation_offset = aml_le16(body + offset + 6);
		out->length = aml_le16(body + offset + 8);
		offset += 10;
	}
	out->source = aml_span(body, length, offset, &out->source_index);
}

static int
aml_res_small(aml_resource_t *out, u8 tag, const u8 *body, u32 length)
{
	out->tag = tag;
	out->large = 0;
	switch (tag) {
	case AML_SMALL_IRQ:
		if (length < 2) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_IRQ;
		out->irq_mask = aml_le16(body);
		out->irq.triggering = AML_TRIGGER_EDGE;
		out->irq.polarity = AML_POLARITY_HIGH;
		out->irq.sharing = AML_SHARING_EXCLUSIVE;
		if (length >= 3) {
			out->irq.triggering = (u8)(body[2] & 0x01);
			out->irq.polarity = (u8)((body[2] >> 3) & 0x01);
			out->irq.sharing = (u8)((body[2] >> 4) & 0x01);
			out->irq.wake_capable = (u8)((body[2] >> 5) & 0x01);
		}
		return (AML_OK);
	case AML_SMALL_DMA:
		if (length < 2) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_DMA;
		out->dma_mask = body[0];
		out->dma_transfer = (u8)(body[1] & 0x03);
		out->bus_master = (u8)((body[1] >> 2) & 0x01);
		out->dma_type = (u8)((body[1] >> 5) & 0x03);
		return (AML_OK);
	case AML_SMALL_START_DEP:
		out->type = AML_RES_START_DEPENDENT;
		if (length >= 1) {
			out->priority = body[0];
		}
		return (AML_OK);
	case AML_SMALL_END_DEP:
		out->type = AML_RES_END_DEPENDENT;
		return (AML_OK);
	case AML_SMALL_IO:
		if (length < 7) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_IO;
		out->resource_type = AML_RES_TYPE_IO;
		out->decode_16 = (u8)(body[0] & 0x01);
		out->minimum = aml_le16(body + 1);
		out->maximum = aml_le16(body + 3);
		out->alignment = body[5];
		out->length = body[6];
		return (AML_OK);
	case AML_SMALL_FIXED_IO:
		if (length < 3) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_FIXED_IO;
		out->resource_type = AML_RES_TYPE_IO;
		out->minimum = (u64)(aml_le16(body) & 0x03FF);
		out->maximum = out->minimum;
		out->min_fixed = 1;
		out->max_fixed = 1;
		out->length = body[2];
		return (AML_OK);
	case AML_SMALL_FIXED_DMA:
		if (length < 5) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_FIXED_DMA;
		out->dma_request = aml_le16(body);
		out->dma_channel = aml_le16(body + 2);
		out->dma_transfer = body[4];
		return (AML_OK);
	case AML_SMALL_VENDOR:
		out->type = AML_RES_VENDOR;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	case AML_SMALL_END:
		out->type = AML_RES_END;
		return (AML_OK);
	default:
		break;
	}
	out->type = AML_RES_UNKNOWN;
	return (AML_OK);
}

static int
aml_res_large(aml_resource_t *out, u8 tag, const u8 *body, u32 length)
{
	out->tag = tag;
	out->large = 1;
	switch (tag) {
	case AML_LARGE_MEMORY24:
		if (length < 9) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_MEMORY;
		out->resource_type = AML_RES_TYPE_MEMORY;
		out->write_protect = (u8)(body[0] & 0x01);
		out->minimum = (u64)aml_le16(body + 1) << 8;
		out->maximum = (u64)aml_le16(body + 3) << 8;
		out->alignment = aml_le16(body + 5);
		out->length = (u64)aml_le16(body + 7) << 8;
		return (AML_OK);
	case AML_LARGE_MEMORY32:
		if (length < 17) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_MEMORY;
		out->resource_type = AML_RES_TYPE_MEMORY;
		out->write_protect = (u8)(body[0] & 0x01);
		out->minimum = aml_le32(body + 1);
		out->maximum = aml_le32(body + 5);
		out->alignment = aml_le32(body + 9);
		out->length = aml_le32(body + 13);
		return (AML_OK);
	case AML_LARGE_FIXED_MEM32:
		if (length < 9) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_MEMORY;
		out->resource_type = AML_RES_TYPE_MEMORY;
		out->write_protect = (u8)(body[0] & 0x01);
		out->minimum = aml_le32(body + 1);
		out->length = aml_le32(body + 5);
		out->maximum = out->minimum;
		out->min_fixed = 1;
		out->max_fixed = 1;
		return (AML_OK);
	case AML_LARGE_GENERIC_REG:
		if (length < 12) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_GENERIC_REG;
		out->space_id = body[0];
		out->bit_width = body[1];
		out->bit_offset = body[2];
		out->access_size = body[3];
		out->minimum = aml_le64(body + 4);
		return (AML_OK);
	case AML_LARGE_VENDOR:
		out->type = AML_RES_VENDOR;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	case AML_LARGE_WORD_SPACE:
		if (length < 13) {
			return (AML_ERR_BOUNDS);
		}
		aml_res_address(out, body, length, 2);
		return (AML_OK);
	case AML_LARGE_DWORD_SPACE:
		if (length < 23) {
			return (AML_ERR_BOUNDS);
		}
		aml_res_address(out, body, length, 4);
		return (AML_OK);
	case AML_LARGE_QWORD_SPACE:
		if (length < 43) {
			return (AML_ERR_BOUNDS);
		}
		aml_res_address(out, body, length, 8);
		return (AML_OK);
	case AML_LARGE_EXT_SPACE:
		if (length < 53) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_ADDRESS;
		out->resource_type = body[0];
		out->producer = (u8)(body[1] & 0x01);
		out->decode_subtractive = (u8)((body[1] >> 1) & 0x01);
		out->min_fixed = (u8)((body[1] >> 2) & 0x01);
		out->max_fixed = (u8)((body[1] >> 3) & 0x01);
		out->type_flags = body[2];
		out->granularity = aml_le64(body + 5);
		out->minimum = aml_le64(body + 13);
		out->maximum = aml_le64(body + 21);
		out->translation_offset = aml_le64(body + 29);
		out->length = aml_le64(body + 37);
		out->attribute = aml_le64(body + 45);
		return (AML_OK);
	case AML_LARGE_EXT_IRQ:
		if (length < 2) {
			return (AML_ERR_BOUNDS);
		}
		out->type = AML_RES_EXT_IRQ;
		out->producer = (u8)(body[0] & 0x01);
		out->irq.triggering = (u8)((body[0] >> 1) & 0x01);
		out->irq.polarity = (u8)((body[0] >> 2) & 0x01);
		out->irq.sharing = (u8)((body[0] >> 3) & 0x01);
		out->irq.wake_capable = (u8)((body[0] >> 4) & 0x01);
		out->irq.is_extended = 1;
		out->gsi_count = body[1];
		if ((u32)out->gsi_count * 4 + 2 > length) {
			return (AML_ERR_BOUNDS);
		}
		out->gsi_table = body + 2;
		out->source = aml_span(body, length,
		    2 + (u32)out->gsi_count * 4, &out->source_index);
		return (AML_OK);
	case AML_LARGE_GPIO:
		out->type = AML_RES_GPIO;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	case AML_LARGE_PIN_FUNCTION:
		out->type = AML_RES_PIN_FUNCTION;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	case AML_LARGE_SERIAL_BUS:
		out->type = AML_RES_SERIAL_BUS;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	case AML_LARGE_PIN_CONFIG:
		out->type = AML_RES_PIN_CONFIG;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	case AML_LARGE_PIN_GROUP:
		out->type = AML_RES_PIN_GROUP;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	case AML_LARGE_CLOCK_INPUT:
		out->type = AML_RES_CLOCK_INPUT;
		out->vendor_data = body;
		out->vendor_length = length;
		return (AML_OK);
	default:
		break;
	}
	out->type = AML_RES_UNKNOWN;
	out->vendor_data = body;
	out->vendor_length = length;
	return (AML_OK);
}

int
aml_resource_walk(aml_object_t *buffer,
    int (*fn)(const aml_resource_t *, void *), void *ctx)
{
	aml_resource_t	resource;
	const u8	*data;
	u32		length;
	u32		offset;
	u32		body;
	u32		total;
	u32		count;
	u8		lead;
	u8		tag;
	int		status;

	buffer = aml_object_deref(buffer);
	if (buffer == NULL || fn == NULL) {
		return (AML_ERR);
	}
	if (buffer->type != AML_TYPE_BUFFER) {
		return (AML_ERR_TYPE);
	}
	data = buffer->u.buffer.data;
	length = buffer->u.buffer.length;
	offset = 0;
	count = 0;
	while (offset < length) {
		if (count >= AML_RES_MAX_DESCRIPTORS) {
			return (AML_ERR_BOUNDS);
		}
		lead = data[offset];
		if ((lead & AML_RES_LARGE_BIT) != 0) {
			if (offset + 3 > length) {
				return (AML_ERR_BOUNDS);
			}
			tag = AML_RES_LARGE_TAG(lead);
			body = aml_le16(data + offset + 1);
			total = body + 3;
		} else {
			tag = AML_RES_SMALL_TAG(lead);
			body = AML_RES_SMALL_LEN(lead);
			total = body + 1;
		}
		if (total == 0 || offset + total > length) {
			return (AML_ERR_BOUNDS);
		}
		memset(&resource, 0, sizeof(resource));
		if ((lead & AML_RES_LARGE_BIT) != 0) {
			status = aml_res_large(&resource, tag,
			    data + offset + 3, body);
		} else {
			status = aml_res_small(&resource, tag,
			    data + offset + 1, body);
		}
		if (status != AML_OK) {
			return (status);
		}
		count++;
		if (fn(&resource, ctx) != 0) {
			return ((int)count);
		}
		if (resource.type == AML_RES_END) {
			return ((int)count);
		}
		offset += total;
	}
	return ((int)count);
}

typedef struct {
	aml_irq_t	*out;
	u32		max;
	u32		count;
} aml_irq_collect_t;

static int
aml_res_emit_irq(aml_irq_collect_t *collect, const aml_resource_t *resource,
    u32 gsi)
{
	aml_irq_t	*slot;

	if (collect->count >= collect->max) {
		return (1);
	}
	slot = &collect->out[collect->count];
	slot->gsi = gsi;
	slot->triggering = resource->irq.triggering;
	slot->polarity = resource->irq.polarity;
	slot->sharing = resource->irq.sharing;
	slot->wake_capable = resource->irq.wake_capable;
	slot->is_extended = resource->irq.is_extended;
	collect->count++;
	return (0);
}

static int
aml_res_collect(const aml_resource_t *resource, void *ctx)
{
	aml_irq_collect_t	*collect;
	u32			i;

	collect = ctx;
	if (resource->type == AML_RES_IRQ) {
		for (i = 0; i < 16; i++) {
			if ((resource->irq_mask & (1U << i)) == 0) {
				continue;
			}
			if (aml_res_emit_irq(collect, resource, i) != 0) {
				return (1);
			}
		}
		return (0);
	}
	if (resource->type == AML_RES_EXT_IRQ) {
		for (i = 0; i < resource->gsi_count; i++) {
			if (aml_res_emit_irq(collect, resource,
			    aml_le32(resource->gsi_table + i * 4)) != 0) {
				return (1);
			}
		}
	}
	return (0);
}

int
aml_resource_irqs(aml_object_t *buffer, aml_irq_t *out, u32 max)
{
	aml_irq_collect_t	collect;
	int			status;

	if (out == NULL || max == 0) {
		return (AML_ERR);
	}
	collect.out = out;
	collect.max = max;
	collect.count = 0;
	status = aml_resource_walk(buffer, aml_res_collect, &collect);
	if (status < 0) {
		return (status);
	}
	return ((int)collect.count);
}
