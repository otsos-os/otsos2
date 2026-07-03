/*
 * Copyright (c) 2026, otsos team
 *
 * [.BSD-2-clause license text...]
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type virtio_gpu_ctrl_hdr_t as packed struct with command header
$define %type virtio_gpu_resp_display_info_t as packed struct with display info
$define %type virtio_gpu_resource_create_2d_t as packed struct with create 2D
$define %type virtio_gpu_resource_unref_t as packed struct with unref
$define %type virtio_gpu_set_scanout_t as packed struct with set scanout
$define %type virtio_gpu_resource_flush_t as packed struct with flush
$define %type virtio_gpu_transfer_to_host_2d_t as packed struct with transfer
$define %type virtio_gpu_resource_attach_backing_t as packed struct with attach backing
$define %type virtio_gpu_mem_entry_t as packed struct with memory entry
$define %type virtio_gpu_resource_detach_backing_t as packed struct with detach backing
$define %type virtio_hw_t as struct with resolved transport state
$define %type virtio_vq_t as struct with virtqueue runtime state

$define %func init_hdr as procedure with args virtio_gpu_ctrl_hdr_t *, u32
$define %func check_response as function with args virtio_gpu_ctrl_hdr_t *, u32
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

$space %internal init_hdr, check_response
$space %export virtio_gpu_cmd_get_display_info
$space %export virtio_gpu_cmd_resource_create_2d
$space %export virtio_gpu_cmd_resource_unref
$space %export virtio_gpu_cmd_set_scanout
$space %export virtio_gpu_cmd_resource_flush
$space %export virtio_gpu_cmd_transfer_to_host_2d
$space %export virtio_gpu_cmd_attach_backing
$space %export virtio_gpu_cmd_detach_backing

*/

#include <kernel/drivers/video/card/virtio-gpu/virtio_gpu_cmds.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static void
init_hdr(virtio_gpu_ctrl_hdr_t *hdr, u32 type)
{
	memset(hdr, 0, sizeof(*hdr));
	hdr->type = type;
}

static int
check_response(virtio_gpu_ctrl_hdr_t *resp, u32 expected)
{
	if (resp->type == expected) {
		return (0);
	}
	drivers_log("[VIRTIO_GPU] response type 0x%x "
	    "(expected 0x%x)\n", resp->type, expected);
	return (-1);
}

int
virtio_gpu_cmd_get_display_info(virtio_hw_t *hw, virtio_vq_t *vq,
    virtio_gpu_resp_display_info_t *info)
{
	virtio_gpu_ctrl_hdr_t	req;
	int			rc;

	init_hdr(&req, VIRTIO_GPU_CMD_GET_DISPLAY_INFO);
	memset(info, 0, sizeof(*info));
	rc = virtio_vq_send_recv(vq, &req, sizeof(req),
	    info, sizeof(*info));
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&info->hdr,
	    VIRTIO_GPU_RESP_OK_DISPLAY_INFO));
}

int
virtio_gpu_cmd_resource_create_2d(virtio_hw_t *hw, virtio_vq_t *vq,
    u32 resource_id, u32 format, u32 width, u32 height)
{
	virtio_gpu_resource_create_2d_t	req;
	virtio_gpu_ctrl_hdr_t		resp;
	int				rc;

	init_hdr(&req.hdr, VIRTIO_GPU_CMD_RESOURCE_CREATE_2D);
	req.resource_id = resource_id;
	req.format = format;
	req.width = width;
	req.height = height;

	memset(&resp, 0, sizeof(resp));
	rc = virtio_vq_send_recv(vq, &req, sizeof(req),
	    &resp, sizeof(resp));
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&resp,
	    VIRTIO_GPU_RESP_OK_NODATA));
}

int
virtio_gpu_cmd_resource_unref(virtio_hw_t *hw, virtio_vq_t *vq,
    u32 resource_id)
{
	virtio_gpu_resource_unref_t	req;
	virtio_gpu_ctrl_hdr_t		resp;
	int				rc;

	init_hdr(&req.hdr, VIRTIO_GPU_CMD_RESOURCE_UNREF);
	req.resource_id = resource_id;
	req.padding = 0;

	memset(&resp, 0, sizeof(resp));
	rc = virtio_vq_send_recv(vq, &req, sizeof(req),
	    &resp, sizeof(resp));
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&resp,
	    VIRTIO_GPU_RESP_OK_NODATA));
}

int
virtio_gpu_cmd_set_scanout(virtio_hw_t *hw, virtio_vq_t *vq,
    u32 scanout_id, u32 resource_id,
    u32 x, u32 y, u32 width, u32 height)
{
	virtio_gpu_set_scanout_t	req;
	virtio_gpu_ctrl_hdr_t		resp;
	int				rc;

	init_hdr(&req.hdr, VIRTIO_GPU_CMD_SET_SCANOUT);
	req.r.x = x;
	req.r.y = y;
	req.r.width = width;
	req.r.height = height;
	req.scanout_id = scanout_id;
	req.resource_id = resource_id;

	memset(&resp, 0, sizeof(resp));
	rc = virtio_vq_send_recv(vq, &req, sizeof(req),
	    &resp, sizeof(resp));
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&resp,
	    VIRTIO_GPU_RESP_OK_NODATA));
}

int
virtio_gpu_cmd_resource_flush(virtio_hw_t *hw, virtio_vq_t *vq,
    u32 resource_id, u32 x, u32 y, u32 width, u32 height)
{
	virtio_gpu_resource_flush_t	req;
	virtio_gpu_ctrl_hdr_t		resp;
	int				rc;

	init_hdr(&req.hdr, VIRTIO_GPU_CMD_RESOURCE_FLUSH);
	req.r.x = x;
	req.r.y = y;
	req.r.width = width;
	req.r.height = height;
	req.resource_id = resource_id;
	req.padding = 0;

	memset(&resp, 0, sizeof(resp));
	rc = virtio_vq_send_recv(vq, &req, sizeof(req),
	    &resp, sizeof(resp));
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&resp,
	    VIRTIO_GPU_RESP_OK_NODATA));
}

int
virtio_gpu_cmd_transfer_to_host_2d(virtio_hw_t *hw,
    virtio_vq_t *vq, u32 resource_id,
    u32 x, u32 y, u32 width, u32 height, u64 offset)
{
	virtio_gpu_transfer_to_host_2d_t	req;
	virtio_gpu_ctrl_hdr_t			resp;
	int					rc;

	init_hdr(&req.hdr,
	    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D);
	req.r.x = x;
	req.r.y = y;
	req.r.width = width;
	req.r.height = height;
	req.offset = offset;
	req.resource_id = resource_id;
	req.padding = 0;

	memset(&resp, 0, sizeof(resp));
	rc = virtio_vq_send_recv(vq, &req, sizeof(req),
	    &resp, sizeof(resp));
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&resp,
	    VIRTIO_GPU_RESP_OK_NODATA));
}

int
virtio_gpu_cmd_attach_backing(virtio_hw_t *hw, virtio_vq_t *vq,
    u32 resource_id, virtio_gpu_mem_entry_t *entries,
    u32 nr_entries)
{
	u32					cmd_size;
	u8					*cmd_buf;
	virtio_gpu_resource_attach_backing_t	*cmd;
	virtio_gpu_mem_entry_t			*ents;
	virtio_gpu_ctrl_hdr_t			resp;
	int					rc;

	if (!entries || nr_entries == 0) {
		return (-1);
	}

	cmd_size = sizeof(virtio_gpu_resource_attach_backing_t) +
	    nr_entries * sizeof(virtio_gpu_mem_entry_t);
	cmd_buf = (u8 *)kmem_alloc(cmd_size);
	if (!cmd_buf) {
		return (-1);
	}

	cmd = (virtio_gpu_resource_attach_backing_t *)cmd_buf;
	init_hdr(&cmd->hdr,
	    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING);
	cmd->resource_id = resource_id;
	cmd->nr_entries = nr_entries;

	ents = (virtio_gpu_mem_entry_t *)(cmd_buf +
	    sizeof(virtio_gpu_resource_attach_backing_t));
	memcpy(ents, entries,
	    nr_entries * sizeof(virtio_gpu_mem_entry_t));

	memset(&resp, 0, sizeof(resp));
	rc = virtio_vq_send_recv(vq, cmd_buf, cmd_size,
	    &resp, sizeof(resp));

	kmem_free(cmd_buf);
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&resp,
	    VIRTIO_GPU_RESP_OK_NODATA));
}

int
virtio_gpu_cmd_detach_backing(virtio_hw_t *hw, virtio_vq_t *vq,
    u32 resource_id)
{
	virtio_gpu_resource_detach_backing_t	req;
	virtio_gpu_ctrl_hdr_t			resp;
	int					rc;

	init_hdr(&req.hdr,
	    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING);
	req.resource_id = resource_id;
	req.padding = 0;

	memset(&resp, 0, sizeof(resp));
	rc = virtio_vq_send_recv(vq, &req, sizeof(req),
	    &resp, sizeof(resp));
	if (rc != 0) {
		return (-1);
	}
	return (check_response(&resp,
	    VIRTIO_GPU_RESP_OK_NODATA));
}
