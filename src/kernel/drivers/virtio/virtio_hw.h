/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type pci_device_t as struct with PCI device info
$define %type virtio_pci_cap_t as packed struct with PCI capability layout
$define %type virtio_pci_notify_cap_t as packed struct extending cap with notify multiplier
$define %type virtio_pci_common_cfg_t as packed struct with common config registers
$define %type virtio_gpu_config_t as packed struct with GPU device config
$define %type virtio_hw_t as struct with resolved transport state

$define %func virtio_hw_init as function with args virtio_hw_t *, pci_device_t *
$define %func virtio_hw_shutdown as procedure with args virtio_hw_t *
$define %func virtio_hw_set_status as procedure with args virtio_hw_t *, u8
$define %func virtio_hw_get_status as function with args virtio_hw_t *
$define %func virtio_hw_get_features as function with args virtio_hw_t *
$define %func virtio_hw_set_features as procedure with args virtio_hw_t *, u32
$define %func virtio_hw_get_features_hi as function with args virtio_hw_t *
$define %func virtio_hw_set_features_hi as procedure with args virtio_hw_t *, u32
$define %func virtio_hw_get_num_queues as function with args virtio_hw_t *
$define %func virtio_hw_select_queue as procedure with args virtio_hw_t *, u16
$define %func virtio_hw_get_queue_size as function with args virtio_hw_t *
$define %func virtio_hw_set_queue_size as procedure with args virtio_hw_t *, u16
$define %func virtio_hw_set_queue_desc as procedure with args virtio_hw_t *, u64
$define %func virtio_hw_set_queue_driver as procedure with args virtio_hw_t *, u64
$define %func virtio_hw_set_queue_device as procedure with args virtio_hw_t *, u64
$define %func virtio_hw_get_queue_notify_off as function with args virtio_hw_t *
$define %func virtio_hw_enable_queue as procedure with args virtio_hw_t *
$define %func virtio_hw_notify_queue as procedure with args virtio_hw_t *, u16
$define %func virtio_hw_read_isr as function with args virtio_hw_t *
$define %func virtio_hw_read_gpu_config as procedure with args virtio_hw_t *, virtio_gpu_config_t *

*/

/* !SPACE!

$space %export virtio_hw_init, virtio_hw_shutdown
$space %export virtio_hw_set_status, virtio_hw_get_status
$space %export virtio_hw_get_features, virtio_hw_set_features
$space %export virtio_hw_get_features_hi, virtio_hw_set_features_hi
$space %export virtio_hw_get_num_queues
$space %export virtio_hw_select_queue, virtio_hw_get_queue_size
$space %export virtio_hw_set_queue_size
$space %export virtio_hw_set_queue_desc, virtio_hw_set_queue_driver
$space %export virtio_hw_set_queue_device
$space %export virtio_hw_get_queue_notify_off
$space %export virtio_hw_enable_queue, virtio_hw_notify_queue
$space %export virtio_hw_read_isr, virtio_hw_read_gpu_config

*/

#ifndef VIRTIO_HW_H
#define VIRTIO_HW_H

#include <kernel/pci/pci.h>
#include <mlibc/mlibc.h>

#define	PCI_CAP_ID_VNDR		0x09
#define	PCI_CFG_CAPABILITIES	0x34

#define	VIRTIO_PCI_CAP_COMMON_CFG		1
#define	VIRTIO_PCI_CAP_NOTIFY_CFG		2
#define	VIRTIO_PCI_CAP_ISR_CFG			3
#define	VIRTIO_PCI_CAP_DEVICE_CFG		4
#define	VIRTIO_PCI_CAP_PCI_CFG			5
#define	VIRTIO_PCI_CAP_SHARED_MEMORY_CFG		8

#define	VIRTIO_STATUS_RESET		0
#define	VIRTIO_STATUS_ACKNOWLEDGE	1
#define	VIRTIO_STATUS_DRIVER		2
#define	VIRTIO_STATUS_DRIVER_OK		4
#define	VIRTIO_STATUS_FEATURES_OK	8
#define	VIRTIO_STATUS_FAILED		128

#define	VIRTIO_GPU_F_VIRGL		0
#define	VIRTIO_GPU_F_EDID		1
#define	VIRTIO_GPU_F_RESOURCE_UUID	2
#define	VIRTIO_GPU_F_RESOURCE_BLOB	3
#define	VIRTIO_GPU_F_CONTEXT_INIT	4

#define	VIRTIO_F_VERSION_1		32

#define	VIRTIO_VENDOR_ID		0x1AF4
#define	VIRTIO_GPU_DEVICE_ID		0x1050

typedef struct {
	u8	cap_vndr;
	u8	cap_next;
	u8	cap_len;
	u8	cfg_type;
	u8	bar;
	u8	id;
	u8	padding[2];
	u32	offset;
	u32	length;
} __attribute__((packed)) virtio_pci_cap_t;

typedef struct {
	virtio_pci_cap_t	cap;
	u32			notify_off_multiplier;
} __attribute__((packed)) virtio_pci_notify_cap_t;

typedef struct {
	u32	device_feature_select;
	u32	device_feature;
	u32	driver_feature_select;
	u32	driver_feature;
	u16	config_msix_vector;
	u16	num_queues;
	u8	device_status;
	u8	config_generation;
	u16	queue_select;
	u16	queue_size;
	u16	queue_msix_vector;
	u16	queue_enable;
	u16	queue_notify_off;
	u64	queue_desc;
	u64	queue_driver;
	u64	queue_device;
	u16	queue_notify_data;
	u16	queue_reset;
} __attribute__((packed)) virtio_pci_common_cfg_t;

typedef struct {
	u32	events_read;
	u32	events_clear;
	u32	num_scanouts;
	u32	num_capsets;
} __attribute__((packed)) virtio_gpu_config_t;

#define	VIRTIO_GPU_EVENT_DISPLAY	(1u << 0)

#define	MMIO_VBASE	0xFFFF800000000000ULL

#define	VIRTIO_GPU_NUM_VQS	2
#define	VIRTIO_GPU_CONTROLQ	0
#define	VIRTIO_GPU_CURSORQ	1

typedef struct {
	pci_device_t		*pci_dev;
	u8			common_is_io;
	u64			common_base;
	u32			common_offset;
	u32			common_length;
	virtio_pci_common_cfg_t	*common_mmio;
	u8			notify_is_io;
	u64			notify_base;
	u32			notify_offset;
	u32			notify_multiplier;
	u8			isr_is_io;
	u64			isr_base;
	u32			isr_offset;
	u8			dev_is_io;
	u64			dev_base;
	u32			dev_offset;
	u32			dev_length;
	virtio_gpu_config_t	*dev_mmio;
	u32			features;
	u8			ready;
} virtio_hw_t;

int	virtio_hw_init(virtio_hw_t *hw, pci_device_t *dev);
void	virtio_hw_shutdown(virtio_hw_t *hw);

void	virtio_hw_set_status(virtio_hw_t *hw, u8 status);
u8	virtio_hw_get_status(virtio_hw_t *hw);

u32	virtio_hw_get_features(virtio_hw_t *hw);
void	virtio_hw_set_features(virtio_hw_t *hw, u32 features);
u32	virtio_hw_get_features_hi(virtio_hw_t *hw);
void	virtio_hw_set_features_hi(virtio_hw_t *hw, u32 features);

u16	virtio_hw_get_num_queues(virtio_hw_t *hw);

void	virtio_hw_select_queue(virtio_hw_t *hw, u16 index);
u16	virtio_hw_get_queue_size(virtio_hw_t *hw);
void	virtio_hw_set_queue_size(virtio_hw_t *hw, u16 size);
void	virtio_hw_set_queue_desc(virtio_hw_t *hw, u64 addr);
void	virtio_hw_set_queue_driver(virtio_hw_t *hw, u64 addr);
void	virtio_hw_set_queue_device(virtio_hw_t *hw, u64 addr);
u16	virtio_hw_get_queue_notify_off(virtio_hw_t *hw);
void	virtio_hw_enable_queue(virtio_hw_t *hw);

void	virtio_hw_notify_queue(virtio_hw_t *hw, u16 queue_index);

u8	virtio_hw_read_isr(virtio_hw_t *hw);
u8	virtio_hw_get_config_generation(virtio_hw_t *hw);
int	virtio_hw_read_device_config(virtio_hw_t *hw, u32 offset,
    void *buf, u32 len);
void	virtio_hw_read_gpu_config(virtio_hw_t *hw,
    virtio_gpu_config_t *out);

#endif
