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

#ifndef API_H
#define API_H

#include <kernel/interrupts/idt.h>
#include <kernel/api/errno.h>
#include <mlibc/mlibc.h>

#define MAX_HANDLES 32
#define MAX_DATA_OBJECTS 64

struct process;

typedef struct {
  int used;
  int flags;
  int object_index;
} api_handle_t;

typedef struct {
  int used;
  int refcount;
  int type;
  char path[256];
  u32 offset;
  int flags;
  void *pipe;
} api_object_t;

#define API_OPEN_READ 0x0001
#define API_OPEN_WRITE 0x0002
#define API_OPEN_RW (API_OPEN_READ | API_OPEN_WRITE)
#define API_OPEN_CREATE 0x0040
#define API_OPEN_TRUNC 0x0200
#define API_OPEN_APPEND 0x0400

#define API_SEEK_SET 0
#define API_SEEK_CUR 1
#define API_SEEK_END 2

#define API_MAP_READ 0x1
#define API_MAP_WRITE 0x2
#define API_MAP_EXEC 0x4

#define API_MAP_PRIVATE 0x02
#define API_MAP_FIXED   0x10
#define API_MAP_ANON    0x20
#define API_MAP_GEM     0x40   /* fd = GEM handle, map buffer into userspace */

#define API_CLONE_VM 0x00000100
#define API_CLONE_THREAD 0x00010000

#define API_OBJECT_FILE 0
#define API_OBJECT_PIPE 1
#define MMAP_BASE 0x0000001000000000ULL
#define MMAP_LIMIT 0x00007FFF00000000ULL

#define PIPE_BUF_SIZE 4096

typedef struct pipe {
  u8 buffer[PIPE_BUF_SIZE];
  u32 read_pos;
  u32 write_pos;
  u32 size;
  int readers;
  int writers;
} pipe_t;

struct api_sysinfo {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
};

struct api_meminfo {
  u64 ram_total_kb;
  u64 ram_free_kb;
  u64 pages_total;
  u64 pages_free;
  u64 pages_active;
  u64 pages_inactive;
  u64 pages_cache;
  u64 pages_wired;
  u64 user_heap_base;
  u64 user_heap_size_kb;
  u64 mmap_base;
  u64 mmap_limit;
};

struct api_kmeminfo {
  u64 kmem_heap_total_kb;
  u64 kmem_heap_used_kb;
  u64 kmem_heap_free_kb;
  u64 bootmem_free_kb;
  u64 kmem_heap_addr;
};

struct api_dirent {
  char name[32];
  u8 type;
  u8 pad[3];
};

struct api_proc_info {
  u32 pid;
  u32 ppid;
  char name[32];
  u32 state;
};

/* DRM sub-operations for CALL_DRM_CALL (passed as `op`).
 *
 * The DRM layer is low-level: it manages GEM memory buffers, KMS objects
 * (framebuffers, planes, CRTCs, connectors) identified by numeric IDs, and
 * atomic state commits. Rendering happens in user memory via rapi helpers
 * or directly; DRM only displays finished buffers.
 *
 * Permission model:
 *   - INFO, GEM_*, FB_*, RAPI_*: available to all processes (they only
 *     touch memory / objects, not the screen).
 *   - ATOMIC_COMMIT page-flip (PLANE_FB_ID on active CRTC): all processes.
 *   - ATOMIC_COMMIT modeset (anything else): kusr only.
 */

/* Queries — everyone */
#define DRM_OP_INFO            1   /* query active mode + driver         */

/* GEM buffer management — everyone */
#define DRM_OP_GEM_CREATE      2   /* allocate buffer, get handle        */
#define DRM_OP_GEM_CLOSE       3   /* release handle                     */
#define DRM_OP_GEM_MAP         4   /* get buffer virtual address + size  */

/* KMS objects — everyone */
#define DRM_OP_FB_CREATE       5   /* wrap GEM as scanout framebuffer    */
#define DRM_OP_FB_DESTROY      6   /* destroy framebuffer                */
#define DRM_OP_GET_OBJECTS     7   /* enumerate plane/crtc/connector IDs */

/* Atomic state — page-flip: everyone, modeset: kusr only */
#define DRM_OP_ATOMIC_COMMIT   8   /* submit property set + present      */

/* rapi — low-level render helpers (write to GEM memory, not screen) — everyone */
#define DRM_OP_RAPI_CLEAR      9   /* clear buffer to color              */
#define DRM_OP_RAPI_PUT_PIXEL  10  /* store one pixel                    */
#define DRM_OP_RAPI_FILL_RECT  11  /* fill rectangle                     */
#define DRM_OP_RAPI_GLYPH      12  /* draw 8x16 glyph                    */
#define DRM_OP_RAPI_SCROLL     13  /* scroll buffer up                   */
#define DRM_OP_RAPI_BLIT       14  /* copy rect between buffers          */

struct api_drm_info {
  u32 available;     /* 1 if DRM is ready                     */
  u32 width;         /* active mode width in pixels           */
  u32 height;        /* active mode height in pixels          */
  u32 pitch;         /* bytes per scanline                    */
  u32 bpp;           /* bits per pixel                        */
  u32 cols;          /* text columns (width / 8)              */
  u32 rows;          /* text rows (height / 16)               */
  char driver_name[32];
};

struct api_drm_gem_create {
  u64 size;          /* in:  requested size in bytes         */
  u32 handle;        /* out: GEM handle (0 = failed)         */
};

struct api_drm_gem_map {
  u32 handle;        /* in:  GEM handle                      */
  u64 vaddr;         /* out: kernel virtual address          */
  u64 size;          /* out: buffer size                     */
};

struct api_drm_fb_create {
  u32 gem_handle;    /* in:  backing GEM buffer              */
  u32 width;         /* in:  framebuffer width               */
  u32 height;        /* in:  framebuffer height              */
  u32 pitch;         /* in:  bytes per scanline              */
  u8 bpp;            /* in:  bits per pixel                  */
  u32 fb_id;         /* out: framebuffer object ID           */
};

struct api_drm_atomic_req {
  u32 obj_id;        /* KMS object ID                        */
  u32 prop_id;       /* property to set                      */
  u64 value;         /* new value                           */
};

struct api_drm_atomic_commit {
  struct api_drm_atomic_req *reqs; /* array of requests          */
  u32 count;                      /* number of requests         */
  u32 flags;                      /* DRM_ATOMIC_* flags         */
};

struct api_drm_objects {
  u32 primary_plane_id;
  u32 crtc_id;
  u32 connector_id;
};

struct api_drm_rapi_pixel {
  u32 handle;        /* GEM buffer to write to               */
  u32 pitch;
  u8 bpp;
  u32 x;
  u32 y;
  u32 color;
};

struct api_drm_rapi_rect {
  u32 handle;
  u32 pitch;
  u8 bpp;
  u32 x;
  u32 y;
  u32 width;
  u32 height;
  u32 color;
};

struct api_drm_rapi_glyph {
  u32 handle;
  u32 pitch;
  u8 bpp;
  u32 x;
  u32 y;
  char c;
  char _pad[3];
  u32 fg;
  u32 bg;
};

struct api_drm_rapi_scroll {
  u32 handle;
  u32 pitch;
  u8 bpp;
  u32 lines;
  u32 bg;
};

struct api_drm_rapi_blit {
  u32 src_handle;
  u32 src_pitch;
  u32 dst_handle;
  u32 dst_pitch;
  u8 bpp;
  u32 sx, sy, sw, sh; /* source rect                         */
  u32 dx, dy;          /* dest origin                         */
};

int api_term_read(void *buf, u32 count);
int api_term_write(const void *buf, u32 count);
int api_data_read(int handle, void *buf, u32 count);
int api_info(struct api_sysinfo *buf);
int api_meminfo(struct api_meminfo *buf);
int api_kmeminfo(struct api_kmeminfo *buf);
void api_info_fill(struct api_sysinfo *buf);
int api_data_write(int handle, const void *buf, u32 count);
int api_data_open(const char *path, int flags);
int api_data_close(int handle);
long api_data_seek(int handle, long offset, int whence);
int api_proc_wait(int *status);
int api_data_pipe(int handles[2]);
long api_proc_clone(u64 flags, u64 child_stack, u64 ptid, registers_t *regs);
u64 api_mem_map(const void *uargs);
int api_mem_unmap(void *addr, u64 length);
int api_proc_copy(registers_t *regs);

int pipe_read(pipe_t *p, void *buf, u32 count);
int pipe_write(pipe_t *p, const void *buf, u32 count);
int api_proc_spawn(const char *path, const char *const *argv,
                   const char *const *envp);
int api_fs_chdir(const char *path);
int api_fs_getcwd(char *buf, u32 size);
int api_fs_listdir(const char *path, struct api_dirent *buf, u32 max_entries);
int api_proc_list(struct api_proc_info *buf, u32 max_entries);
int api_kusr_auth(const char *password);
int api_drm_call(u64 op, void *arg);
void api_init(void);
void api_init_process(struct process *proc);
void api_copy_handles(struct process *dst, const struct process *src);
void api_release_handles(struct process *proc);
api_handle_t *api_get_handle_table(void);
api_object_t *api_get_object_table(void);
int api_alloc_object(void);
void api_release_object(int index);
int api_proc_getpid(void);
int api_proc_getppid(void);

#endif
