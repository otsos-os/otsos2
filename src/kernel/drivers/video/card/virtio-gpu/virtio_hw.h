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

/*
 * virtio PCI modern transport — capability parsing and register access.
 *
 * The virtio PCI device exposes vendor-specific capabilities (cap_vndr = 0x09)
 * that point to the common configuration, notification, ISR, and device-specific
 * structures inside a BAR. This header defines the on-wire layout of those
 * structures and the API to initialise the transport from a pci_device_t.
 */

#ifndef VIRTIO_HW_H
#define VIRTIO_HW_H

#include <kernel/pci/pci.h>
#include <mlibc/mlibc.h>

/* PCI capability IDs and offsets not in pci.h. */
#define PCI_CAP_ID_VNDR    0x09
#define PCI_CFG_CAPABILITIES 0x34

/* virtio capability types (cfg_type field). */
#define VIRTIO_PCI_CAP_COMMON_CFG      1
#define VIRTIO_PCI_CAP_NOTIFY_CFG      2
#define VIRTIO_PCI_CAP_ISR_CFG         3
#define VIRTIO_PCI_CAP_DEVICE_CFG      4
#define VIRTIO_PCI_CAP_PCI_CFG         5
#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8

/* Device status bits (device_status field in common cfg). */
#define VIRTIO_STATUS_RESET       0
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED      128

/* virtio-gpu feature bits. */
#define VIRTIO_GPU_F_VIRGL          0
#define VIRTIO_GPU_F_EDID           1
#define VIRTIO_GPU_F_RESOURCE_UUID  2
#define VIRTIO_GPU_F_RESOURCE_BLOB  3
#define VIRTIO_GPU_F_CONTEXT_INIT   4

/* Generic virtio feature bits we care about. */
#define VIRTIO_F_VERSION_1          32

/* PCI vendor/device IDs for virtio-gpu. */
#define VIRTIO_VENDOR_ID    0x1AF4
#define VIRTIO_GPU_DEVICE_ID 0x1050

/* On-wire capability structure (read from PCI config space). */
typedef struct {
  u8 cap_vndr;
  u8 cap_next;
  u8 cap_len;
  u8 cfg_type;
  u8 bar;
  u8 id;
  u8 padding[2];
  u32 offset;
  u32 length;
} __attribute__((packed)) virtio_pci_cap_t;

/* Notification capability has an extra field after virtio_pci_cap_t. */
typedef struct {
  virtio_pci_cap_t cap;
  u32 notify_off_multiplier;
} __attribute__((packed)) virtio_pci_notify_cap_t;

/* Common configuration structure (mapped in BAR memory). */
typedef struct {
  u32 device_feature_select;
  u32 device_feature;
  u32 driver_feature_select;
  u32 driver_feature;
  u16 config_msix_vector;
  u16 num_queues;
  u8 device_status;
  u8 config_generation;
  u16 queue_select;
  u16 queue_size;
  u16 queue_msix_vector;
  u16 queue_enable;
  u16 queue_notify_off;
  u64 queue_desc;
  u64 queue_driver;
  u64 queue_device;
  u16 queue_notify_data;
  u16 queue_reset;
} __attribute__((packed)) virtio_pci_common_cfg_t;

/* virtio-gpu device-specific configuration (mapped in device BAR). */
typedef struct {
  u32 events_read;
  u32 events_clear;
  u32 num_scanouts;
  u32 num_capsets;
} __attribute__((packed)) virtio_gpu_config_t;

#define VIRTIO_GPU_EVENT_DISPLAY (1u << 0)

/*
 * MMIO regions from 64-bit PCI BARs can land at physical addresses above
 * 512 GB (e.g. 0xC000000000).  Such addresses fall in PML4 index 1, which
 * is in the user-space range and NOT copied to user process page tables by
 * pmap_create().  To make MMIO accessible from any context (including
 * kshell running under a user CR3), we remap every MMIO region into the
 * kernel half of the address space, starting at PML4 index 256
 * (0xFFFF800000000000).  This range is shared across all processes.
 */
#define MMIO_VBASE 0xFFFF800000000000ULL

/* Maximum number of virtqueues we manage (controlq + cursorq). */
#define VIRTIO_GPU_NUM_VQS 2
#define VIRTIO_GPU_CONTROLQ 0
#define VIRTIO_GPU_CURSORQ  1

/*
 * Resolved transport state — the driver's view of where each capability
 * structure lives in the address space. For MMIO BARs the base is the
 * identity-mapped physical address; for I/O BARs it is the port number.
 */
typedef struct {
  pci_device_t *pci_dev;

  /* Common configuration. */
  u8   common_is_io;
  u64  common_base;
  u32  common_offset;
  u32  common_length;
  virtio_pci_common_cfg_t *common_mmio;

  /* Notification. */
  u8   notify_is_io;
  u64  notify_base;
  u32  notify_offset;
  u32  notify_multiplier;

  /* ISR status. */
  u8   isr_is_io;
  u64  isr_base;
  u32  isr_offset;

  /* Device-specific configuration (virtio-gpu config). */
  u8   dev_is_io;
  u64  dev_base;
  u32  dev_offset;
  virtio_gpu_config_t *dev_mmio;

  /* Negotiated features. */
  u32  features;

  u8   ready;
} virtio_hw_t;

/* Initialise the transport from a probed PCI device. Returns 0 on success. */
int virtio_hw_init(virtio_hw_t *hw, pci_device_t *dev);

/* Tear down the transport (reset the device). */
void virtio_hw_shutdown(virtio_hw_t *hw);

/* --- Common config register accessors --- */

void virtio_hw_set_status(virtio_hw_t *hw, u8 status);
u8   virtio_hw_get_status(virtio_hw_t *hw);

u32  virtio_hw_get_features(virtio_hw_t *hw);
void virtio_hw_set_features(virtio_hw_t *hw, u32 features);
u32  virtio_hw_get_features_hi(virtio_hw_t *hw);
void virtio_hw_set_features_hi(virtio_hw_t *hw, u32 features);

u16  virtio_hw_get_num_queues(virtio_hw_t *hw);

/* Virtqueue configuration: select, set size, set addresses, enable. */
void virtio_hw_select_queue(virtio_hw_t *hw, u16 index);
u16  virtio_hw_get_queue_size(virtio_hw_t *hw);
void virtio_hw_set_queue_size(virtio_hw_t *hw, u16 size);
void virtio_hw_set_queue_desc(virtio_hw_t *hw, u64 addr);
void virtio_hw_set_queue_driver(virtio_hw_t *hw, u64 addr);
void virtio_hw_set_queue_device(virtio_hw_t *hw, u64 addr);
u16  virtio_hw_get_queue_notify_off(virtio_hw_t *hw);
void virtio_hw_enable_queue(virtio_hw_t *hw);

/* Notify the device that a queue has pending buffers. */
void virtio_hw_notify_queue(virtio_hw_t *hw, u16 queue_index);

/* Read the ISR status register (resets the register on read). */
u8 virtio_hw_read_isr(virtio_hw_t *hw);

/* Read the device-specific config (virtio-gpu config). */
void virtio_hw_read_gpu_config(virtio_hw_t *hw, virtio_gpu_config_t *out);

#endif
