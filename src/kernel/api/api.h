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
#include <kernel/api/input_abi.h>
#include <kernel/api/session.h>
#include <kernel/entity/entity.h>
#include <kernel/drivers/keyboard/keycodes.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/video/drm/kms/property.h>
#include <mlibc/mlibc.h>

struct process;
struct api_ipc_message;
struct api_ipc_call;

#define API_OPEN_READ 0x0001
#define API_OPEN_WRITE 0x0002
#define API_OPEN_RW (API_OPEN_READ | API_OPEN_WRITE)
#define API_OPEN_CREATE 0x0040
#define API_OPEN_TRUNC 0x0200
#define API_OPEN_APPEND 0x0400

#define API_DATA_DIR_MKDIR	1
#define API_DATA_DIR_RMDIR	2
#define API_DATA_DIR_RENAME	3

#define API_SEEK_SET 0
#define API_SEEK_CUR 1
#define API_SEEK_END 2

#define API_MAP_READ 0x1
#define API_MAP_WRITE 0x2
#define API_MAP_EXEC 0x4

#define API_MAP_SHARED  0x01
#define API_MAP_PRIVATE 0x02
#define API_MAP_FIXED   0x10
#define API_MAP_ANON    0x20
#define API_MAP_GEM     0x40   /* fd = GEM handle, map buffer into userspace */

#define API_CLONE_VM 0x00000100
#define API_CLONE_THREAD 0x00010000

#define MMAP_BASE 0x0000001000000000ULL
#define MMAP_LIMIT 0x00007FFF00000000ULL

#define API_TERM_POWER_GET	0
#define API_TERM_POWER_CHANGE	1
#define API_TERM_POWER_RESET	2
#define API_TERM_MOUSE_UPDATE	0
#define API_TERM_MOUSE_VISIBLE	0x00000001

#define API_TERM_ACTIVE		(-1)
#define API_TERM_MODE_GET	0
#define API_TERM_MODE_SET	1
#define API_TERM_NCCS		32

#define API_TERM_IFLAG_IGNBRK	0000001
#define API_TERM_IFLAG_BRKINT	0000002
#define API_TERM_IFLAG_IGNPAR	0000004
#define API_TERM_IFLAG_PARMRK	0000010
#define API_TERM_IFLAG_INPCK	0000020
#define API_TERM_IFLAG_ISTRIP	0000040
#define API_TERM_IFLAG_INLCR	0000100
#define API_TERM_IFLAG_IGNCR	0000200
#define API_TERM_IFLAG_ICRNL	0000400
#define API_TERM_IFLAG_IXON	0001000
#define API_TERM_IFLAG_IXOFF	0002000
#define API_TERM_IFLAG_IXANY	0004000
#define API_TERM_IFLAG_IMAXBEL	0010000
#define API_TERM_IFLAG_IUTF8	0040000

#define API_TERM_OFLAG_OPOST	0000001
#define API_TERM_OFLAG_OLCUC	0000002
#define API_TERM_OFLAG_ONLCR	0000004
#define API_TERM_OFLAG_OCRNL	0000010
#define API_TERM_OFLAG_ONOCR	0000020
#define API_TERM_OFLAG_ONLRET	0000040
#define API_TERM_OFLAG_OFILL	0000100
#define API_TERM_OFLAG_OFDEL	0000200

#define API_TERM_CFLAG_CBAUD	0010017
#define API_TERM_CFLAG_CSIZE	0000060
#define API_TERM_CFLAG_CS5	0000000
#define API_TERM_CFLAG_CS6	0000020
#define API_TERM_CFLAG_CS7	0000040
#define API_TERM_CFLAG_CS8	0000060
#define API_TERM_CFLAG_CSTOPB	0000100
#define API_TERM_CFLAG_CREAD	0000200
#define API_TERM_CFLAG_PARENB	0000400
#define API_TERM_CFLAG_PARODD	0001000
#define API_TERM_CFLAG_HUPCL	0002000
#define API_TERM_CFLAG_CLOCAL	0004000

#define API_TERM_LFLAG_ISIG	0000001
#define API_TERM_LFLAG_ICANON	0000002
#define API_TERM_LFLAG_XCASE	0000004
#define API_TERM_LFLAG_ECHO	0000010
#define API_TERM_LFLAG_ECHOE	0000020
#define API_TERM_LFLAG_ECHOK	0000040
#define API_TERM_LFLAG_ECHONL	0000100
#define API_TERM_LFLAG_NOFLSH	0000200
#define API_TERM_LFLAG_TOSTOP	0000400
#define API_TERM_LFLAG_ECHOCTL	0001000
#define API_TERM_LFLAG_ECHOPRT	0002000
#define API_TERM_LFLAG_ECHOKE	0004000
#define API_TERM_LFLAG_FLUSHO	0010000
#define API_TERM_LFLAG_PENDIN	0040000
#define API_TERM_LFLAG_IEXTEN	0100000

#define API_TERM_CC_VINTR	0
#define API_TERM_CC_VQUIT	1
#define API_TERM_CC_VERASE	2
#define API_TERM_CC_VKILL	3
#define API_TERM_CC_VEOF	4
#define API_TERM_CC_VTIME	5
#define API_TERM_CC_VMIN	6
#define API_TERM_CC_VSTART	8
#define API_TERM_CC_VSTOP	9
#define API_TERM_CC_VSUSP	10
#define API_TERM_CC_VEOL	11
#define API_TERM_CC_VREPRINT	12
#define API_TERM_CC_VDISCARD	13
#define API_TERM_CC_VWERASE	14
#define API_TERM_CC_VLNEXT	15
#define API_TERM_CC_VEOL2	16

#define API_TERM_SPEED_B38400	0000015

#define TERM_STATE_ACTIVE	0
#define TERM_STATE_SUSPENDED	1
#define TERM_STATE_DISABLED	2

struct api_term_power {
	int	op;
	int	tty;
	int	state;
	int	flags;
};

struct api_term_mouse {
	int	op;
	int	tty;
	int	flags;
	int	x;
	int	y;
	int	buttons;
};

struct api_term_mode {
	int	op;
	int	tty;
	u32	iflag;
	u32	oflag;
	u32	cflag;
	u32	lflag;
	u8	cc[API_TERM_NCCS];
	u32	ispeed;
	u32	ospeed;
};

#define	API_INPUT_NONBLOCK	0x00000001

struct api_key_event {
	u64	timestamp;
	u16	key;
	u16	raw;
	u32	flags;
	u32	mods;
	u32	ch;
};

struct api_term_info {
	int	tty;
	int	state;
	u16	rows;
	u16	cols;
	u16	xpixel;
	u16	ypixel;
};

#define	API_FS_TYPE_REG		1
#define	API_FS_TYPE_DIR		2
#define	API_FS_TYPE_CHR		3
#define	API_FS_TYPE_PIPE	4
#define	API_FS_TYPE_LNK		5

#define	API_MS_RDONLY		VFS_MNT_RDONLY
#define	API_MS_NOSUID		VFS_MNT_NOSUID
#define	API_MS_NODEV		VFS_MNT_NODEV
#define	API_MS_NOEXEC		VFS_MNT_NOEXEC
#define	API_MS_SYNCHRONOUS	VFS_MNT_SYNCHRONOUS
#define	API_MS_MANDLOCK		VFS_MNT_MANDLOCK
#define	API_MS_DIRSYNC		VFS_MNT_DIRSYNC
#define	API_MS_NOATIME		VFS_MNT_NOATIME
#define	API_MS_NODIRATIME	VFS_MNT_NODIRATIME
#define	API_MS_RELATIME		VFS_MNT_RELATIME
#define	API_MS_KUSR_ONLY	VFS_MNT_KUSR_ONLY

struct api_fs_stat {
	u32	type;
	u32	mode;
	u32	uid;
	u32	gid;
	u64	size;
	u64	blocks;
	s64	atime;
	s64	mtime;
	s64	ctime;
	char	name[32];
};

/* Decoding helpers for EVFILT_KBD kevent data. */
#define KBD_DATA_KEY(v)		((u16)((u64)(v) & 0xFFFF))
#define KBD_DATA_RELEASED(v)	(((u64)(v) >> 16) & 1)
#define KBD_DATA_EXTENDED(v)	(((u64)(v) >> 17) & 1)
#define KBD_DATA_ASCII(v)	((char)(((u64)(v) >> 24) & 0xFF))

#define	MOUSE_BUTTON_LEFT	0x00000001
#define	MOUSE_BUTTON_RIGHT	0x00000002
#define	MOUSE_BUTTON_MIDDLE	0x00000004
#define	MOUSE_BUTTON_X1		0x00000008
#define	MOUSE_BUTTON_X2		0x00000010

#define	MOUSE_EVENT_MOVE	0x00000001
#define	MOUSE_EVENT_BUTTON	0x00000002
#define	MOUSE_EVENT_WHEEL	0x00000004
#define	MOUSE_EVENT_OVERFLOW	0x00000008

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

#define	API_NET_ADDR_IP4		1
#define	API_NET_PROTO_UDP		1
#define	API_NET_PROTO_TCP		2
#define	API_NET_MODE_DGRAM		1
#define	API_NET_MODE_STREAM		2
#define	API_NET_OPEN_NONBLOCK		0x00000001
#define	API_NET_MSG_NONBLOCK		0x00000001
#define	API_NET_MSG_TRUNC		0x00000002
#define	API_NET_CTL_COMMON_BASE		0x0000
#define	API_NET_CTL_PROTO_BASE		0x1000
#define	API_NET_CTL_PRIV_BASE		0x8000
#define	API_NET_CTL_GET_LOCAL		(API_NET_CTL_COMMON_BASE + 1)
#define	API_NET_CTL_GET_PEER		(API_NET_CTL_COMMON_BASE + 2)
#define	API_NET_CTL_GET_IFACE		(API_NET_CTL_COMMON_BASE + 3)

struct api_net_addr {
	u32	family;
	u32	port;
	u32	ip;
	u32	ifindex;
};

struct api_net_iface {
	u32	ifindex;
	u32	flags;
	u32	ip;
	u32	netmask;
	u32	gateway;
	u32	mtu;
	u8	mac[6];
	u8	pad[2];
	char	name[16];
	char	device[16];
};

struct api_net_msg {
	void			*data;
	struct api_net_addr	*addr;
	u32			length;
	u32			flags;
};

#define	API_REG_OPEN_READ	API_OPEN_READ
#define	API_REG_OPEN_WRITE	API_OPEN_WRITE
#define	API_REG_OPEN_RW		API_OPEN_RW
#define	API_REG_OPEN_CREATE	API_OPEN_CREATE

#define	API_REG_TYPE_STRING		1
#define	API_REG_TYPE_BOOL		2
#define	API_REG_TYPE_I32		3
#define	API_REG_TYPE_U32		4
#define	API_REG_TYPE_U64		5
#define	API_REG_TYPE_IPV4		6
#define	API_REG_TYPE_BYTES		7
#define	API_REG_TYPE_MULTI_STRING	8

#define	API_REG_KIND_KEY	1
#define	API_REG_KIND_VALUE	2

#define	API_REG_CONSUMER_NET		1
#define	API_REG_CONSUMER_SCHEDULER	2
#define	API_REG_CONSUMER_KUSR		3

#define	API_KOFO_NAME_MAX	32
#define	API_KOFO_VERSION_MAX	32
#define	API_KOFO_PATH_MAX	128
#define	API_KOFO_STATE_EMPTY	0
#define	API_KOFO_STATE_LOADING	1
#define	API_KOFO_STATE_LOADED	2
#define	API_KOFO_STATE_UNLOADING	3

struct api_kofo_info {
	u32	size;
	u32	id;
	u32	state;
	u32	flags;
	u64	image_base;
	u64	image_size;
	u32	section_count;
	u32	symbol_count;
	u32	import_count;
	u32	reloc_count;
	u32	driver_count;
	u32	pad;
	char	name[API_KOFO_NAME_MAX];
	char	version[API_KOFO_VERSION_MAX];
	char	path[API_KOFO_PATH_MAX];
};

struct api_reg_value {
	const char	*name;
	void		*data;
	u32		size;
	u32		type;
	u32		flags;
	u32		bytes;
};

struct api_reg_entry {
	u32	index;
	u32	kind;
	u32	type;
	u32	size;
	char	name[32];
};

#define API_CPUINFO_MAX_CPUS 32
#define API_CPUINFO_MAX_PIDS 64
struct api_cpu_entry {
  u32 cpu_index;
  u32 lapic_id;
  u32 present;
  u32 online;
  u32 pid;
  u32 tid;
  u32 state;
  u32 pid_count;
  u32 pids[API_CPUINFO_MAX_PIDS];
  char proc_name[32];
};
struct api_cpuinfo {
  u32 cpu_count;
  u32 entry_count;
  struct api_cpu_entry entries[API_CPUINFO_MAX_CPUS];
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
#define API_PROC_SPAWN_ABI_POSIX	0
#define API_PROC_SPAWN_ABI_NATIVE	1
#define	API_PROC_PERM_USER		0
#define	API_PROC_PERM_KUSR		1

struct api_proc_spawn_args {
	u32			size;
	u32			flags;
	u32			abi;
	u32			pad;
	const char		*path;
	const char *const	*argv;
	const char *const	*envp;
};

/* DRM sub-operations for CALL_DRM_CALL (passed as `op`).
 *
 * The DRM layer is a low-level display manager: it owns GEM memory buffers,
 * KMS objects (framebuffers, planes, CRTCs, connectors) as flat arrays of
 * IDs and properties, and a data-oriented atomic state. Rendering happens in
 * user memory via rapi helpers or directly; DRM only validates and commits
 * the finished state to a backend driver.
 *
 * Permission model:
 *   - INFO, GEM_CREATE/CLOSE, FB_CREATE/DESTROY, GET_OBJECTS, RAPI_*:
 *     available to all processes (they only touch memory / objects).
 *   - ATOMIC_COMMIT page-flip (PLANE_FB_ID / damage props on active CRTC):
 *     available to all processes.
 *   - ATOMIC_COMMIT modeset (CRTC, connector, or plane binding changes):
 *     requires kusr / DRM master.
 *   - DRIVER_LIST: available to all processes.
 *   - DRIVER_SWITCH: kusr only.
 *
 * Plane property IDs for atomic requests are exported below and mirror
 * <kernel/drivers/video/drm/kms/property.h>.
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

#define DRM_OP_DRIVER_SWITCH   15
#define DRM_OP_DRIVER_LIST     16 

#define DRM_PROP_PLANE_FB_ID      1
#define DRM_PROP_PLANE_CRTC_ID    2
#define DRM_PROP_PLANE_SRC_X      3
#define DRM_PROP_PLANE_SRC_Y      4
#define DRM_PROP_PLANE_SRC_W      5
#define DRM_PROP_PLANE_SRC_H      6
#define DRM_PROP_PLANE_CRTC_X     7
#define DRM_PROP_PLANE_CRTC_Y     8
#define DRM_PROP_PLANE_CRTC_W     9
#define DRM_PROP_PLANE_CRTC_H     10
#define DRM_PROP_PLANE_DIRTY_X    11
#define DRM_PROP_PLANE_DIRTY_Y    12
#define DRM_PROP_PLANE_DIRTY_W    13
#define DRM_PROP_PLANE_DIRTY_H    14

struct api_drm_info {
  u32 available;     /* 1 if DRM is ready                     */
  u32 width;         /* active mode width in pixels           */
  u32 height;        /* active mode height in pixels          */
  u32 pitch;         /* bytes per scanline                    */
  u32 bpp;           /* bits per pixel                        */
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
  u32 cursor_plane_id;
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

struct api_drm_driver_entry {
  u32 id;
  char name[32];
  u32 active;
};

struct api_drm_driver_list {
  struct api_drm_driver_entry *entries;
  u32 max_entries;
  u32 count;
};

struct api_drm_driver_switch {
  u32 id;
};

struct api_timeinfo {
  u64 wall_sec;
  u64 wall_nsec;
  u64 local_sec;
  u64 local_nsec;
  u64 uptime_sec;
  u64 uptime_nsec;
  u64 ticks;
  u64 frequency;
  s64 timezone_offset;
  char clocksource[32];
};

#define	API_TRACE_MAX_CPUS		32
#define	API_TRACE_MAX_PROVIDERS		16
#define	API_TRACE_MAX_PROBES		256
#define	API_TRACE_MAX_ARGS		8
#define	API_TRACE_MAX_PREDICATES	8
#define	API_TRACE_MAX_ACTIONS		16
#define	API_TRACE_MAX_PROGRAMS		128
#define	API_TRACE_MAX_AGGREGATIONS	256
#define	API_TRACE_RECORD_STACK		8
#define	API_TRACE_NAME_LEN		32
#define	API_TRACE_MAX_PMU_COUNTERS	16
#define	API_TRACE_READ_MAX_RECORDS	4096

#define	API_TRACE_OPEN_PRIVILEGED	0x00000001
#define	API_TRACE_OPEN_KERNEL_STACK	0x00000002

#define	API_TRACE_CLEAR_RECORDS		0x00000001
#define	API_TRACE_CLEAR_PROGRAMS		0x00000002
#define	API_TRACE_CLEAR_AGGS		0x00000004
#define	API_TRACE_CLEAR_ALL \
	(API_TRACE_CLEAR_RECORDS | API_TRACE_CLEAR_PROGRAMS | \
	API_TRACE_CLEAR_AGGS)

#define	API_TRACE_REC_F_USER		0x00000001
#define	API_TRACE_REC_F_KERNEL_STACK	0x00000002
#define	API_TRACE_REC_F_PMU_VALID	0x00000004
#define	API_TRACE_REC_F_DROPPED_BEFORE	0x00000008

#define	API_TRACE_ARG_U64		1
#define	API_TRACE_ARG_S64		2
#define	API_TRACE_ARG_PID		3
#define	API_TRACE_ARG_TID		4
#define	API_TRACE_ARG_CPU		5
#define	API_TRACE_ARG_ID		6
#define	API_TRACE_ARG_PTR		7
#define	API_TRACE_ARG_CYCLES		8
#define	API_TRACE_ARG_ERRNO		9
#define	API_TRACE_ARG_FLAGS		10
#define	API_TRACE_ARG_BYTES		11

#define	API_TRACE_FIELD_NONE		0
#define	API_TRACE_FIELD_PID		1
#define	API_TRACE_FIELD_TID		2
#define	API_TRACE_FIELD_CPU		3
#define	API_TRACE_FIELD_PROBE		4
#define	API_TRACE_FIELD_ARG0		16
#define	API_TRACE_FIELD_ARG1		17
#define	API_TRACE_FIELD_ARG2		18
#define	API_TRACE_FIELD_ARG3		19
#define	API_TRACE_FIELD_ARG4		20
#define	API_TRACE_FIELD_ARG5		21
#define	API_TRACE_FIELD_ARG6		22
#define	API_TRACE_FIELD_ARG7		23

#define	API_TRACE_PRED_EQ		1
#define	API_TRACE_PRED_NE		2
#define	API_TRACE_PRED_LT		3
#define	API_TRACE_PRED_LE		4
#define	API_TRACE_PRED_GT		5
#define	API_TRACE_PRED_GE		6
#define	API_TRACE_PRED_MASK		7

#define	API_TRACE_ACT_RECORD		1
#define	API_TRACE_ACT_STACK		2
#define	API_TRACE_ACT_COUNT		3
#define	API_TRACE_ACT_SUM		4
#define	API_TRACE_ACT_MIN		5
#define	API_TRACE_ACT_MAX		6
#define	API_TRACE_ACT_QUANTIZE		7
#define	API_TRACE_ACT_LQUANTIZE		8

#define	API_TRACE_OP_START		1
#define	API_TRACE_OP_STOP		2
#define	API_TRACE_OP_LOAD		3
#define	API_TRACE_OP_CLEAR		4

#define	API_TRACE_INFO_STATS		1
#define	API_TRACE_INFO_PROVIDERS	2
#define	API_TRACE_INFO_PROBES		3
#define	API_TRACE_INFO_PMU		4
#define	API_TRACE_INFO_AGGS		5

#define	API_TRACE_PMU_CYCLES		0
#define	API_TRACE_PMU_INSTRUCTIONS	1
#define	API_TRACE_PMU_CACHE_REFERENCES	2
#define	API_TRACE_PMU_CACHE_MISSES	3
#define	API_TRACE_PMU_BRANCH_INSTRUCTIONS	4
#define	API_TRACE_PMU_BRANCH_MISSES	5
#define	API_TRACE_PMU_COUNTER_COUNT	6

struct api_trace_arg {
	char	name[API_TRACE_NAME_LEN];
	u32	type;
	u32	flags;
};

struct api_trace_provider {
	u32	id;
	u32	enabled;
	u32	probe_count;
	u32	reserved;
	char	name[API_TRACE_NAME_LEN];
};

struct api_trace_providers {
	struct api_trace_provider	*providers;
	u32				max_providers;
	u32				count;
};

struct api_trace_probe {
	u32	id;
	u32	provider;
	u32	enabled;
	u32	argc;
	u32	flags;
	u32	reserved;
	char	provider_name[API_TRACE_NAME_LEN];
	char	module[API_TRACE_NAME_LEN];
	char	function[API_TRACE_NAME_LEN];
	char	name[API_TRACE_NAME_LEN];
	struct api_trace_arg args[API_TRACE_MAX_ARGS];
};

struct api_trace_probes {
	struct api_trace_probe	*probes;
	u32			max_probes;
	u32			count;
};

struct api_trace_predicate {
	u32	field;
	u32	op;
	u64	value;
};

struct api_trace_action {
	u32	kind;
	u32	arg;
	u32	key;
	u32	id;
	u64	value;
};

struct api_trace_program {
	u32				probe_id;
	u32				flags;
	u32				predicate_count;
	u32				action_count;
	struct api_trace_predicate	predicates[API_TRACE_MAX_PREDICATES];
	struct api_trace_action		actions[API_TRACE_MAX_ACTIONS];
};

struct api_trace_load {
	struct api_trace_program	*programs;
	u32			program_count;
	u32			flags;
};

struct api_trace_record {
	u64	seq;
	u64	tsc;
	u64	ticks;
	u64	pid;
	u64	tid;
	u64	ip;
	u64	sp;
	u64	bp;
	u64	probe_id;
	u64	action_id;
	u64	args[API_TRACE_MAX_ARGS];
	u64	stack[API_TRACE_RECORD_STACK];
	u32	cpu;
	u32	flags;
	u32	argc;
	u32	stack_count;
};

struct api_trace_read {
	struct api_trace_record	*records;
	u32			max_records;
	u32			records_read;
	u64			records_total;
	u64			records_lost;
	u32			flags;
	u32			reserved;
};

struct api_trace_agg {
	u32	id;
	u32	kind;
	u32	probe_id;
	u32	arg;
	u64	key[4];
	u64	value;
	u64	count;
};

struct api_trace_aggs {
	struct api_trace_agg	*aggs;
	int			trace;
	u32			max_aggs;
	u32			count;
	u32			clear;
	u32			reserved;
};

struct api_trace_stats {
	u64	records_written;
	u64	records_lost;
	u64	probe_hits[API_TRACE_MAX_PROBES];
	u64	action_hits;
	u64	aggregation_updates;
	u32	provider_count;
	u32	probe_count;
	u32	session_count;
	u32	ring_records;
	u32	enabled;
	u32	initialized;
};

struct api_trace_session_stats {
	u64	records_written;
	u64	records_read;
	u64	records_lost;
	u64	aggregation_count;
	u32	active;
	u32	program_count;
	u32	flags;
	u32	reserved;
};

struct api_trace_pmu_counter {
	u32	id;
	u32	enabled;
	char	name[API_TRACE_NAME_LEN];
};

struct api_trace_pmu {
	struct api_trace_pmu_counter	*counters;
	u32				max_counters;
	u32				count;
	u32				events_enabled;
	u32				reserved;
};

int api_term_read(void *buf, u32 count, u32 flags);
int api_term_write(const void *buf, u32 count);
int api_data_read(int handle, void *buf, u32 count);
int api_info(struct api_sysinfo *buf);
int api_meminfo(struct api_meminfo *buf);
int api_kmeminfo(struct api_kmeminfo *buf);
int api_cpuinfo(struct api_cpuinfo *buf);
void api_info_fill(struct api_sysinfo *buf);
int api_data_write(int handle, const void *buf, u32 count);
int api_data_open(const char *path, int flags);
int api_data_close(int handle);
long api_data_seek(int handle, long offset, int whence);
int api_proc_wait(int *status);
int api_data_pipe(int handles[2]);
int api_data_dir(u32 op, const char *path, const char *newpath);
long api_proc_clone(u64 flags, u64 child_stack, u64 ptid, registers_t *regs);
u64 api_mem_map(const void *uargs);
int api_mem_unmap(void *addr, u64 length);
int api_proc_copy(registers_t *regs);

int pipe_read(pipe_t *p, void *buf, u32 count);
int pipe_write(pipe_t *p, const void *buf, u32 count);
int api_proc_spawn(const struct api_proc_spawn_args *uargs);
int api_fs_chdir(const char *path);
int api_fs_getcwd(char *buf, u32 size);
int api_fs_listdir(const char *path, struct api_dirent *buf, u32 max_entries);
int api_fs_stat(const char *path, struct api_fs_stat *buf);
int api_fs_rename(const char *oldpath, const char *newpath);
int api_fs_unlink(const char *path);
#define	API_LINK_SYMLINK	0
#define	API_LINK_HARD		1

int api_fs_linknew(const char *target, const char *linkpath, u32 flags);
int api_fs_linkgo(const char *path, char *buf, u32 bufsize);
int api_fs_mnt(const char *source, const char *target, const char *fstype,
    u64 flags, const void *data);
int api_fs_umnt(const char *target, u64 flags);
int api_proc_list(struct api_proc_info *buf, u32 max_entries);
int api_kusr_auth(const char *password);
int api_drm_call(u64 op, void *arg);
void api_init(void);
void api_init_process(struct process *proc);
void api_copy_handles(struct process *dst, const struct process *src);
void api_release_handles(struct process *proc);
int api_proc_getpid(void);
int api_proc_getppid(void);
int api_proc_gettid(void);
int api_proc_perm(u32 pid);
void api_thread_exit(int code);
int api_thread_join(u32 tid, int *status);
void api_proc_exit_group(int code);
int api_proc_set_tid_address(u64 tidptr);
int api_session_setsid(void);
int api_session_getsid(void);
void api_session_init(struct process *proc);
void api_session_fork(struct process *parent, struct process *child);
int api_futex_wait(u64 uaddr, u32 expected_val);
int api_futex_wake(u64 uaddr, u32 max_waiters);
int api_sys_random(u8 *buf, u32 len);
int api_kofo_load(const char *path, u32 flags);
int api_kofo_info(u32 id, struct api_kofo_info *info);
int api_kofo_unload(u32 id, u32 flags);
int api_timeinfo(struct api_timeinfo *buf);
int api_time(void);
int api_term_power(struct api_term_power *args);
int api_term_info(struct api_term_info *info);
int api_term_mouse(struct api_term_mouse *args);
int api_term_mode(struct api_term_mode *args);
int api_input_read(struct api_key_event *buf, u32 count, u32 flags);
int api_input_poll(void);
int api_input_flush(void);
int api_net_open(int proto, int mode, u32 flags);
int api_net_bind(int handle, const struct api_net_addr *uaddr);
int api_net_connect(int handle, const struct api_net_addr *uaddr);
int api_net_listen(int handle, int backlog);
int api_net_accept(int handle, struct api_net_addr *uaddr, u32 flags);
int api_net_send(int handle, const struct api_net_msg *umsg);
int api_net_recv(int handle, struct api_net_msg *umsg);
int api_net_ctl(int handle, int op, void *arg);
int api_ipc_create(const char *name, u32 flags, u32 mode);
int api_ipc_connect(const char *name, u32 flags);
int api_ipc_send(int handle, const struct api_ipc_message *message);
int api_ipc_recv(int handle, struct api_ipc_message *message, u32 flags);
int api_ipc_call(int handle, struct api_ipc_call *call);
int api_ipc_ctl(int handle, u32 op, void *arg);
int api_reg_open(const char *hive, const char *key, u32 flags);
int api_reg_close(int handle);
int api_reg_get(int handle, struct api_reg_value *uvalue);
int api_reg_set(int handle, const struct api_reg_value *uvalue);
int api_reg_create_key(int handle, const char *name);
int api_reg_delete_key(int handle, const char *name);
int api_reg_delete_value(int handle, const char *name);
int api_reg_enum(int handle, struct api_reg_entry *uentry);
int api_reg_upd(u32 consumer);
int api_trace_open(u32 flags);
int api_trace_close(int trace);
int api_trace_read(int trace, struct api_trace_read *args);
int api_trace_ctl(int trace, u32 op, void *arg);
int api_trace_info(u32 op, void *arg);
int api_trace_mark(u32 id, u64 a0, u64 a1, u64 a2, u64 a3, u64 a4);
void api_trace_cleanup_process(struct process *proc);

/* Entity manager (native object namespace) */
#define	API_ENTITY_NAME_MAX		64
#define	API_ENTITY_LIST_MAX_ENTRIES	256

#define	ENTITY_IO_PTR_BACKING		0
#define	ENTITY_IO_PTR_PATH		1
#define	ENTITY_IO_I32_OFFSET		0
#define	ENTITY_IO_I32_FLAGS		1

#define	ENTITY_CTL_GET_INFO		1
#define	ENTITY_CTL_GET_DATA		2
#define	ENTITY_CTL_SET_DATA		3
#define	ENTITY_CTL_GET_I32		4
#define	ENTITY_CTL_SET_I32		5
#define	ENTITY_CTL_BIND			6
#define	ENTITY_CTL_UNBIND		7
#define	ENTITY_CTL_DELETE		8

struct api_entity_create_args {
	u16		archetype;
	u16		flags;
	u32		access;
	const char	*name;
};

struct api_entity_stat {
	u64		id;
	u16		archetype;
	u16		state;
	u32		flags;
	s32		refs;
	u32		owner_pid;
	u32		uid;
	u32		gid;
	u32		euid;
	u32		egid;
	u64		size;
	u64		created;
	char		name[API_ENTITY_NAME_MAX];
};

struct api_entity_entry {
	u64		id;
	u16		archetype;
	u16		state;
	u32		owner_pid;
	char		name[API_ENTITY_NAME_MAX];
};

struct api_entity_data {
	u32		index;
	u32		pad;
	u64		value;
};

struct api_entity_list {
	const char		*path;
	struct api_entity_entry	*entries;
	u32			max_entries;
	u32			count;
};

struct api_entity_query {
	u16			archetype;
	u16			pad;
	u32			start;
	struct api_entity_entry	*entries;
	u32			max_entries;
	u32			count;
};

int api_entity_create(const struct api_entity_create_args *uargs);
int api_entity_open(const char *uname, u32 access);
int api_entity_close(int handle);
int api_entity_dup(int handle, u32 access);
int api_entity_stat(int handle, struct api_entity_stat *ustat);
int api_entity_list(const struct api_entity_list *ulist);
int api_entity_query(const struct api_entity_query *uquery);
int api_entity_ctl(int handle, u32 op, void *uarg);

/* Entity-backed IO helpers used by the native API layer */
void	entity_io_init(void);
entity_id_t entity_io_create_raw(u16 arch, u32 flags);
int	entity_io_attach(entity_id_t id, u32 access);
int	entity_io_open_id(entity_id_t id, u32 access);
void	*entity_io_ptr(entity_id_t id, u32 index);
int	entity_io_set_ptr(entity_id_t id, u32 index, void *ptr);
int	entity_io_i32(entity_id_t id, u32 index, s32 *value);
int	entity_io_set_i32(entity_id_t id, u32 index, s32 value);

#endif
