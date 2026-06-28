/*
 * Copyright (c) 2026, otsos team
 *
 * [.BSD-2-clause license text...]
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type virtio_gpu_ctrl_hdr_t as packed struct with command header
$define %type virtio_gpu_rect_t as packed struct with rectangle
$define %type virtio_gpu_resp_display_info_t as packed struct with display info response
$define %type virtio_gpu_resource_create_2d_t as packed struct with create 2D command
$define %type virtio_gpu_resource_unref_t as packed struct with unref command
$define %type virtio_gpu_set_scanout_t as packed struct with set scanout command
$define %type virtio_gpu_resource_flush_t as packed struct with flush command
$define %type virtio_gpu_transfer_to_host_2d_t as packed struct with transfer command
$define %type virtio_gpu_resource_attach_backing_t as packed struct with attach backing command
$define %type virtio_gpu_mem_entry_t as packed struct with memory entry
$define %type virtio_gpu_resource_detach_backing_t as packed struct with detach backing command
$define %type virtio_hw_t as struct with resolved transport state
$define %type virtio_vq_t as struct with virtqueue runtime state

$define %func virtio_gpu_cmd_get_display_info as function with args virtio_hw_t *, virtio_vq_t *, virtio_gpu_resp_display_info_t *
$define %func virtio_gpu_cmd_resource_create_2d as function with args virtio_hw_t *, virtio_vq_t *, u32, u32, u32, u32
$define %func virtio_gpu_cmd_resource_unref as function with args virtio_hw_t *, virtio_vq_t *, u32
$define %func virtio_gpu_cmd_set_scanout as function with args virtio_hw_t *, virtio_vq_t *, u32, u32, u32, u32, u32, u32
$define %func virtio_gpu_cmd_resource_flush as function with args virtio_hw_t *, virtio_vq_t *, u32, u32, u32, u32, u32
$define %func virtio_gpu_cmd_transfer_to_host_2d as function with args virtio_hw_t *, virtio_vq_t *, u32, u32, u32, u32, u32, u64
$define %func virtio_gpu_cmd_attach_backing as function with args virtio_hw_t *, virtio_vq_t *, u32, virtio_gpu_mem_entry_t *, u32
$define %func virtio_gpu_cmd_detach_backing as function with args virtio_hw_t *, virtio_vq_t *, u32

*/

/* !SPACE!

$space %export virtio_gpu_cmd_get_display_info
$space %export virtio_gpu_cmd_resource_create_2d
$space %export virtio_gpu_cmd_resource_unref
$space %export virtio_gpu_cmd_set_scanout
$space %export virtio_gpu_cmd_resource_flush
$space %export virtio_gpu_cmd_transfer_to_host_2d
$space %export virtio_gpu_cmd_attach_backing
$space %export virtio_gpu_cmd_detach_backing

*/

#ifndef VIRTIO_GPU_CMDS_H
#define VIRTIO_GPU_CMDS_H

#include <mlibc/mlibc.h>

#define	VIRTIO_GPU_CMD_GET_DISPLAY_INFO		0x0100
#define	VIRTIO_GPU_CMD_RESOURCE_CREATE_2D	0x0101
#define	VIRTIO_GPU_CMD_RESOURCE_UNREF		0x0102
#define	VIRTIO_GPU_CMD_SET_SCANOUT		0x0103
#define	VIRTIO_GPU_CMD_RESOURCE_FLUSH		0x0104
#define	VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D	0x0105
#define	VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING	0x0106
#define	VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING	0x107

#define	VIRTIO_GPU_CMD_UPDATE_CURSOR		0x0300
#define	VIRTIO_GPU_CMD_MOVE_CURSOR		0x0301

#define	VIRTIO_GPU_RESP_OK_NODATA		0x1100
#define	VIRTIO_GPU_RESP_OK_DISPLAY_INFO		0x1101

#define	VIRTIO_GPU_RESP_ERR_UNSPEC		0x1200
#define	VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY	0x1201
#define	VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID	0x1202
#define	VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID	0x1203
#define	VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID	0x1204
#define	VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER	0x1205

#define	VIRTIO_GPU_FLAG_FENCE	1

#define	VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM	1
#define	VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM	2
#define	VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM	3
#define	VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM	4
#define	VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM	67

#define	VIRTIO_GPU_MAX_SCANOUTS	16

typedef struct {
	u32	type;
	u32	flags;
	u64	fence_id;
	u32	ctx_id;
	u8	ring_idx;
	u8	padding[3];
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

typedef struct {
	u32	x;
	u32	y;
	u32	width;
	u32	height;
} __attribute__((packed)) virtio_gpu_rect_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	u32			scanout;
	u32			padding;
} __attribute__((packed)) virtio_gpu_get_edid_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	struct {
		virtio_gpu_rect_t	r;
		u32			enabled;
		u32			flags;
	} pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	u32			resource_id;
	u32			format;
	u32			width;
	u32			height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	u32			resource_id;
	u32			padding;
} __attribute__((packed)) virtio_gpu_resource_unref_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	virtio_gpu_rect_t	r;
	u32			scanout_id;
	u32			resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	virtio_gpu_rect_t	r;
	u32			resource_id;
	u32			padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	virtio_gpu_rect_t	r;
	u64			offset;
	u32			resource_id;
	u32			padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	u32			resource_id;
	u32			nr_entries;
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

typedef struct {
	u64	addr;
	u32	length;
	u32	padding;
} __attribute__((packed)) virtio_gpu_mem_entry_t;

typedef struct {
	virtio_gpu_ctrl_hdr_t	hdr;
	u32			resource_id;
	u32			padding;
} __attribute__((packed)) virtio_gpu_resource_detach_backing_t;

#include <kernel/drivers/video/card/virtio-gpu/virtio_hw.h>
#include <kernel/drivers/video/card/virtio-gpu/virtio_queue.h>

int	virtio_gpu_cmd_get_display_info(virtio_hw_t *hw,
    virtio_vq_t *vq, virtio_gpu_resp_display_info_t *info);
int	virtio_gpu_cmd_resource_create_2d(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 resource_id, u32 format,
    u32 width, u32 height);
int	virtio_gpu_cmd_resource_unref(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 resource_id);
int	virtio_gpu_cmd_set_scanout(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 scanout_id, u32 resource_id,
    u32 x, u32 y, u32 width, u32 height);
int	virtio_gpu_cmd_resource_flush(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 resource_id,
    u32 x, u32 y, u32 width, u32 height);
int	virtio_gpu_cmd_transfer_to_host_2d(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 resource_id,
    u32 x, u32 y, u32 width, u32 height, u64 offset);
int	virtio_gpu_cmd_attach_backing(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 resource_id,
    virtio_gpu_mem_entry_t *entries, u32 nr_entries);
int	virtio_gpu_cmd_detach_backing(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 resource_id);

#endif
