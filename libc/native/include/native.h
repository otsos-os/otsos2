/* !DEFINES!

$define %type api_sysinfo as struct with kernel identity strings
$define %type api_kmeminfo as struct with native kernel memory data
$define %type api_fs_stat as struct with native file metadata
$define %type kevent as struct with native event data
$define %func termWrite as function with args const void *, size_t
$define %func dataOpen as function with args const char *, int
$define %func procSpawn as function with args const char *, char *const *, char *const *

*/

/* !SPACE!

$space %export termRead, termReadFlags, termWrite, termPrint
$space %export dataOpen, dataClose, dataRead, dataWrite, dataReadFull
$space %export dataWriteFull, dataSeek, dataPipe
$space %export fsChdir, fsGetcwd, fsListdir, fsStat, fsRename, fsUnlink
$space %export procSpawn, procWait, procRun, procExit, procKill
$space %export memMap, memUnmap, eventKqueue, eventWait, eventClose
$space %export drmCall, drmInfo, drmGemCreate, drmGemClose, drmGemMapInfo
$space %export drmGemMmap, drmFbCreate, drmFbDestroy, drmGetObjects
$space %export drmAtomicCommit, drmRapiClear, drmRapiPutPixel
$space %export drmRapiFillRect, drmRapiGlyph, drmRapiScroll, drmRapiBlit
$space %export drmDriverList, drmDriverSwitch

*/

#ifndef _NATIVE_H
#define _NATIVE_H

#include <stddef.h>
#include <stdint.h>

#define CALL_TERM_READ		0x100
#define CALL_TERM_WRITE		0x101
#define CALL_TERM_INFO		0x102
#define CALL_TERM_POWER		0x111
#define TERM_READ_IGNORE_SIGINT	0x00000001
#define TERM_READ_NO_ECHO	0x00000002
#define CALL_INPUT_READ		0x120
#define CALL_INPUT_POLL		0x121
#define CALL_INPUT_FLUSH	0x122
#define CALL_DATA_OPEN		0x200
#define CALL_DATA_CLOSE		0x201
#define CALL_DATA_READ		0x202
#define CALL_DATA_WRITE		0x203
#define CALL_DATA_SEEK		0x204
#define CALL_DATA_PIPE		0x205
#define CALL_FS_CHDIR		0x206
#define CALL_FS_GETCWD		0x207
#define CALL_FS_LISTDIR		0x208
#define CALL_FS_STAT		0x209
#define CALL_FS_RENAME		0x20A
#define CALL_FS_UNLINK		0x20B
#define CALL_FS_LINKNEW		0x20C
#define CALL_FS_LINKGO		0x20D
#define CALL_MEM_MAP		0x300
#define CALL_MEM_UNMAP		0x301
#define CALL_PROC_CLONE		0x400
#define CALL_PROC_COPY		0x401
#define CALL_PROC_SPAWN		0x402
#define CALL_PROC_EXIT		0x403
#define CALL_PROC_WAIT		0x404
#define CALL_PROC_KILL		0x405
#define CALL_PROC_LIST		0x406
#define CALL_KUSR_AUTH		0x407
#define CALL_PROC_GETPID	0x408
#define CALL_PROC_GETPPID	0x409
#define CALL_PROC_THREAD_EXIT	0x40A
#define CALL_PROC_THREAD_JOIN	0x40B
#define CALL_PROC_GETTID	0x40C
#define CALL_PROC_EXIT_GROUP	0x40D
#define CALL_PROC_SET_TID_ADDR	0x40E
#define CALL_FUTEX_WAIT		0x40F
#define CALL_FUTEX_WAKE		0x410
#define CALL_PROC_SETSID	0x411
#define CALL_PROC_GETSID	0x412
#define CALL_SYS_INFO		0x500
#define CALL_SYS_MEMINFO	0x501
#define CALL_SYS_KMEMINFO	0x502
#define CALL_SYS_RANDOM		0x503
#define CALL_SYS_TIMEINFO	0x504
#define CALL_SYS_TIME		0x505
#define CALL_SYS_CPUINFO	0x506
#define CALL_DRM_CALL		0x600
#define CALL_EVENT_KQUEUE	0x700
#define CALL_EVENT_KEVENT	0x701
#define CALL_EVENT_CLOSE	0x702
#define CALL_PERSONALITY	0xFFFF

#define API_OPEN_READ		0x0001
#define API_OPEN_WRITE		0x0002
#define API_OPEN_RW		(API_OPEN_READ | API_OPEN_WRITE)
#define API_OPEN_CREATE		0x0040
#define API_OPEN_TRUNC		0x0200
#define API_OPEN_APPEND		0x0400

#define API_SEEK_SET		0
#define API_SEEK_CUR		1
#define API_SEEK_END		2

#define API_MAP_READ		0x1
#define API_MAP_WRITE		0x2
#define API_MAP_EXEC		0x4
#define API_MAP_SHARED		0x01
#define API_MAP_PRIVATE		0x02
#define API_MAP_FIXED		0x10
#define API_MAP_ANON		0x20
#define API_MAP_GEM		0x40

#define API_CLONE_VM		0x00000100
#define API_CLONE_THREAD	0x00010000

#define API_INPUT_NONBLOCK	0x00000001

#define API_FS_TYPE_REG		1
#define API_FS_TYPE_DIR		2
#define API_FS_TYPE_CHR		3
#define API_FS_TYPE_PIPE	4
#define API_FS_TYPE_LNK		5

#define EVFILT_READ		(-1)
#define EVFILT_WRITE		(-2)
#define EVFILT_TIMER		(-3)
#define EVFILT_PROC		(-4)
#define EVFILT_SIGNAL		(-5)
#define EVFILT_USER		(-6)
#define EVFILT_KBD		(-7)
#define EVFILT_MOUSE		(-8)

#define EV_ADD			0x0001
#define EV_DELETE		0x0002
#define EV_ENABLE		0x0004
#define EV_DISABLE		0x0008
#define EV_ONESHOT		0x0010
#define EV_CLEAR		0x0020
#define EV_EOF			0x8000

#define NOTE_EXIT		0x80000000U

#define DRM_OP_INFO		1
#define DRM_OP_GEM_CREATE	2
#define DRM_OP_GEM_CLOSE	3
#define DRM_OP_GEM_MAP		4
#define DRM_OP_FB_CREATE	5
#define DRM_OP_FB_DESTROY	6
#define DRM_OP_GET_OBJECTS	7
#define DRM_OP_ATOMIC_COMMIT	8
#define DRM_OP_RAPI_CLEAR	9
#define DRM_OP_RAPI_PUT_PIXEL	10
#define DRM_OP_RAPI_FILL_RECT	11
#define DRM_OP_RAPI_GLYPH	12
#define DRM_OP_RAPI_SCROLL	13
#define DRM_OP_RAPI_BLIT	14
#define DRM_OP_DRIVER_SWITCH	15
#define DRM_OP_DRIVER_LIST	16

#define KBD_DATA_KEY(v)		((uint16_t)((uint64_t)(v) & 0xFFFF))
#define KBD_DATA_RELEASED(v)	(((uint64_t)(v) >> 16) & 1)
#define KBD_DATA_EXTENDED(v)	(((uint64_t)(v) >> 17) & 1)
#define KBD_DATA_ASCII(v)	((char)(((uint64_t)(v) >> 24) & 0xFF))

#define MOUSE_DATA_DX(v)	((int16_t)((uint64_t)(v) & 0xFFFF))
#define MOUSE_DATA_DY(v)	((int16_t)(((uint64_t)(v) >> 16) & 0xFFFF))
#define MOUSE_DATA_DZ(v)	((int8_t)(((uint64_t)(v) >> 32) & 0xFF))
#define MOUSE_DATA_BUTTONS(v)	((uint8_t)(((uint64_t)(v) >> 40) & 0xFF))
#define MOUSE_DATA_FLAGS(v)	((uint16_t)(((uint64_t)(v) >> 48) & 0xFFFF))

struct api_sysinfo {
	char	sysname[65];
	char	nodename[65];
	char	release[65];
	char	version[65];
	char	machine[65];
	char	domainname[65];
};

struct api_meminfo {
	uint64_t	ram_total_kb;
	uint64_t	ram_free_kb;
	uint64_t	pages_total;
	uint64_t	pages_free;
	uint64_t	pages_active;
	uint64_t	pages_inactive;
	uint64_t	pages_cache;
	uint64_t	pages_wired;
	uint64_t	user_heap_base;
	uint64_t	user_heap_size_kb;
	uint64_t	mmap_base;
	uint64_t	mmap_limit;
};
struct api_kmeminfo {
	uint64_t	kmem_heap_total_kb;
	uint64_t	kmem_heap_used_kb;
	uint64_t	kmem_heap_free_kb;
	uint64_t	bootmem_free_kb;
	uint64_t	kmem_heap_addr;
};

struct api_dirent {
	char		name[32];
	uint8_t		type;
	uint8_t		pad[3];
};

struct api_fs_stat {
	uint32_t	type;
	uint32_t	mode;
	uint32_t	uid;
	uint32_t	gid;
	uint64_t	size;
	uint64_t	blocks;
	int64_t		atime;
	int64_t		mtime;
	int64_t		ctime;
	char		name[32];
};

#define API_CPUINFO_MAX_CPUS 32
#define API_CPUINFO_MAX_PIDS 64

struct api_cpu_entry {
	uint32_t	cpu_index;
	uint32_t	lapic_id;
	uint32_t	present;
	uint32_t	online;
	uint32_t	pid;
	uint32_t	tid;
	uint32_t	state;
	uint32_t	pid_count;
	uint32_t	pids[API_CPUINFO_MAX_PIDS];
	char		proc_name[32];
};

struct api_cpuinfo {
	uint32_t	cpu_count;
	uint32_t	entry_count;
	struct api_cpu_entry entries[API_CPUINFO_MAX_CPUS];
};

struct api_proc_info {
	uint32_t	pid;
	uint32_t	ppid;
	char		name[32];
	uint32_t	state;
};

struct api_timeinfo {
	uint64_t	wall_sec;
	uint64_t	wall_nsec;
	uint64_t	local_sec;
	uint64_t	local_nsec;
	uint64_t	uptime_sec;
	uint64_t	uptime_nsec;
	uint64_t	ticks;
	uint64_t	frequency;
	int64_t		timezone_offset;
	char		clocksource[32];
};

struct api_key_event {
	uint64_t	timestamp;
	uint16_t	key;
	uint16_t	raw;
	uint32_t	flags;
	uint32_t	mods;
	uint32_t	ch;
};

struct api_term_info {
	int	tty;
	int	state;
	uint16_t rows;
	uint16_t cols;
	uint16_t xpixel;
	uint16_t ypixel;
};

struct api_term_power {
	int	op;
	int	tty;
	int	state;
	int	flags;
};

struct mem_map_args {
	uint64_t	addr;
	uint64_t	length;
	uint32_t	prot;
	uint32_t	flags;
	int		fd;
	uint64_t	offset;
} __attribute__((packed));

struct kevent {
	uint64_t	ident;
	int16_t		filter;
	uint16_t	flags;
	uint32_t	fflags;
	int64_t		data;
	uint64_t	udata;
};

struct api_drm_info {
	uint32_t	available;
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	uint32_t	bpp;
	char		driver_name[32];
};

struct api_drm_gem_create {
	uint64_t	size;
	uint32_t	handle;
};

struct api_drm_gem_map {
	uint32_t	handle;
	uint64_t	vaddr;
	uint64_t	size;
};

struct api_drm_fb_create {
	uint32_t	gem_handle;
	uint32_t	width;
	uint32_t	height;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	fb_id;
};

struct api_drm_atomic_req {
	uint32_t	obj_id;
	uint32_t	prop_id;
	uint64_t	value;
};

struct api_drm_atomic_commit {
	struct api_drm_atomic_req *reqs;
	uint32_t	count;
	uint32_t	flags;
};

struct api_drm_objects {
	uint32_t	primary_plane_id;
	uint32_t	cursor_plane_id;
	uint32_t	crtc_id;
	uint32_t	connector_id;
};

struct api_drm_rapi_pixel {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	x;
	uint32_t	y;
	uint32_t	color;
};

struct api_drm_rapi_rect {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	x;
	uint32_t	y;
	uint32_t	width;
	uint32_t	height;
	uint32_t	color;
};

struct api_drm_rapi_glyph {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	x;
	uint32_t	y;
	char		c;
	char		_pad[3];
	uint32_t	fg;
	uint32_t	bg;
};

struct api_drm_rapi_scroll {
	uint32_t	handle;
	uint32_t	pitch;
	uint8_t		bpp;
	uint32_t	lines;
	uint32_t	bg;
};

struct api_drm_rapi_blit {
	uint32_t	src_handle;
	uint32_t	src_pitch;
	uint32_t	dst_handle;
	uint32_t	dst_pitch;
	uint8_t		bpp;
	uint32_t	sx;
	uint32_t	sy;
	uint32_t	sw;
	uint32_t	sh;
	uint32_t	dx;
	uint32_t	dy;
};

struct api_drm_driver_entry {
	uint32_t	id;
	char		name[32];
	uint32_t	active;
};

struct api_drm_driver_list {
	struct api_drm_driver_entry *entries;
	uint32_t	max_entries;
	uint32_t	count;
};

struct api_drm_driver_switch {
	uint32_t	id;
};

ssize_t	termRead(void *buf, size_t count);
ssize_t	termReadFlags(void *buf, size_t count, uint32_t flags);
ssize_t	termWrite(const void *buf, size_t count);
ssize_t	termPrint(const char *text);
int	termInfo(struct api_term_info *info);
int	termPower(struct api_term_power *args);
int	inputRead(struct api_key_event *buf, uint32_t count, uint32_t flags);
int	inputPoll(void);
int	inputFlush(void);

int	dataOpen(const char *path, int flags);
int	dataClose(int handle);
ssize_t	dataRead(int handle, void *buf, size_t count);
ssize_t	dataWrite(int handle, const void *buf, size_t count);
int	dataReadFull(int handle, void *buf, size_t count);
int	dataWriteFull(int handle, const void *buf, size_t count);
long	dataSeek(int handle, long offset, int whence);
int	dataPipe(int handles[2]);

int	fsChdir(const char *path);
int	fsGetcwd(char *buf, size_t size);
int	fsListdir(const char *path, struct api_dirent *buf, uint32_t max_entries);
int	fsStat(const char *path, struct api_fs_stat *buf);
int	fsRename(const char *oldpath, const char *newpath);
int	fsUnlink(const char *path);
int	fsLinkNew(const char *target, const char *linkpath, uint32_t flags);
int	fsLinkGo(const char *path, char *buf, uint32_t bufsize);

void	*memMap(const struct mem_map_args *args);
int	memUnmap(void *addr, size_t length);

long	procClone(uint64_t flags, void *child_stack, uint64_t ptid);
int	procCopy(void);
int	procSpawn(const char *path, char *const argv[], char *const envp[]);
int	procWait(int *status);
int	procRun(const char *path, char *const argv[], char *const envp[],
	    int *status);
void	procExit(int code) __attribute__((noreturn));
int	procKill(uint32_t pid, int sig);
int	procList(struct api_proc_info *buf, uint32_t max_entries);
int	procGetpid(void);
int	procGetppid(void);
int	procGettid(void);
void	threadExit(int code) __attribute__((noreturn));
int	threadJoin(uint32_t tid, int *status);
void	procExitGroup(int code) __attribute__((noreturn));
int	procSetTidAddress(uint64_t tidptr);
int	procSetsid(void);
int	procGetsid(void);

int	futexWait(uint64_t uaddr, uint32_t expected_val);
int	futexWake(uint64_t uaddr, uint32_t max_waiters);
int	kusrAuth(const char *password);

int	sysInfo(struct api_sysinfo *buf);
int	sysMemInfo(struct api_meminfo *buf);
int	sysKmemInfo(struct api_kmeminfo *buf);
int	sysCpuInfo(struct api_cpuinfo *buf);
int	sysRandom(void *buf, size_t len);
int	sysTimeInfo(struct api_timeinfo *buf);
int	sysTime(void);

int	drmCall(uint64_t op, void *arg);
int	drmInfo(struct api_drm_info *info);
int	drmGemCreate(size_t size, uint32_t *handle);
int	drmGemClose(uint32_t handle);
int	drmGemMapInfo(uint32_t handle, struct api_drm_gem_map *info);
void	*drmGemMmap(uint32_t handle, size_t size, uint32_t prot);
int	drmFbCreate(uint32_t gem_handle, uint32_t width, uint32_t height,
	    uint32_t pitch, uint8_t bpp, uint32_t *fb_id);
int	drmFbDestroy(uint32_t fb_id);
int	drmGetObjects(struct api_drm_objects *objects);
int	drmAtomicCommit(struct api_drm_atomic_req *reqs, uint32_t count,
	    uint32_t flags);
int	drmRapiClear(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t color);
int	drmRapiPutPixel(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t x, uint32_t y, uint32_t color);
int	drmRapiFillRect(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t x, uint32_t y, uint32_t width, uint32_t height,
	    uint32_t color);
int	drmRapiGlyph(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
int	drmRapiScroll(uint32_t handle, uint32_t pitch, uint8_t bpp,
	    uint32_t lines, uint32_t bg);
int	drmRapiBlit(struct api_drm_rapi_blit *blit);
int	drmDriverList(struct api_drm_driver_entry *entries,
	    uint32_t max_entries, uint32_t *count);
int	drmDriverSwitch(uint32_t id);

int	eventKqueue(void);
int	eventClose(int kq);
int	eventWait(int kq, struct kevent *changes, int nchanges,
	    struct kevent *events, int nevents, int64_t timeout_ms);

long	personality(long mode);

#endif
