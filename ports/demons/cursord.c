/* native mouse cursor daemon for otsos2*/

#define	CALL_TERM_WRITE		0x101
#define	CALL_PROC_EXIT		0x403
#define	CALL_PROC_COPY		0x401
#define	CALL_PROC_SETSID		0x411
#define	CALL_DRM_CALL		0x600
#define	CALL_EVENT_KQUEUE		0x700
#define	CALL_EVENT_KEVENT		0x701
#define	CALL_PERSONALITY		0xFFFF

#define	EVFILT_MOUSE		(-8)
#define	EV_ADD			0x0001
#define	EV_CLEAR		0x0020

#define	DRM_OP_INFO		1
#define	DRM_OP_GEM_CREATE	2
#define	DRM_OP_FB_CREATE	5
#define	DRM_OP_GET_OBJECTS	7
#define	DRM_OP_ATOMIC_COMMIT	8
#define	DRM_OP_RAPI_CLEAR	9
#define	DRM_OP_RAPI_PUT_PIXEL	10

#define	DRM_PROP_PLANE_FB_ID	1
#define	DRM_PROP_PLANE_SRC_X	3
#define	DRM_PROP_PLANE_SRC_Y	4
#define	DRM_PROP_PLANE_SRC_W	5
#define	DRM_PROP_PLANE_SRC_H	6
#define	DRM_PROP_PLANE_CRTC_X	7
#define	DRM_PROP_PLANE_CRTC_Y	8
#define	DRM_PROP_PLANE_CRTC_W	9
#define	DRM_PROP_PLANE_CRTC_H	10

#define	CURSOR_W		24
#define	CURSOR_H		32
#define	CURSOR_PITCH		(CURSOR_W * 4)
#define	CURSOR_EVENT_BATCH	32

#define	MOUSE_DATA_DX(v)	((s16)((u64)(v) & 0xFFFF))
#define	MOUSE_DATA_DY(v)	((s16)(((u64)(v) >> 16) & 0xFFFF))

typedef unsigned long long	u64;
typedef unsigned int		u32;
typedef unsigned short		u16;
typedef unsigned char		u8;
typedef long long		s64;
typedef signed short		s16;

struct kevent {
	u64	ident;
	short	filter;
	u16	flags;
	u32	fflags;
	s64	data;
	u64	udata;
};

struct kevent_args {
	int		kq_idx;
	struct kevent	*changelist;
	int		nchanges;
	struct kevent	*eventlist;
	int		nevents;
	s64		timeout_ms;
};

struct api_drm_info {
	u32	available;
	u32	width;
	u32	height;
	u32	pitch;
	u32	bpp;
	char	driver_name[32];
};

struct api_drm_gem_create {
	u64	size;
	u32	handle;
};

struct api_drm_fb_create {
	u32	gem_handle;
	u32	width;
	u32	height;
	u32	pitch;
	u8	bpp;
	u32	fb_id;
};

struct api_drm_objects {
	u32	primary_plane_id;
	u32	cursor_plane_id;
	u32	crtc_id;
	u32	connector_id;
};

struct api_drm_atomic_req {
	u32	obj_id;
	u32	prop_id;
	u64	value;
};

struct api_drm_atomic_commit {
	struct api_drm_atomic_req	*reqs;
	u32				count;
	u32				flags;
};

struct api_drm_rapi_pixel {
	u32	handle;
	u32	pitch;
	u8	bpp;
	u32	x;
	u32	y;
	u32	color;
};

struct api_drm_rapi_rect {
	u32	handle;
	u32	pitch;
	u8	bpp;
	u32	x;
	u32	y;
	u32	width;
	u32	height;
	u32	color;
};

static long
syscall0(long num)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num)
	    : "rcx", "r11", "memory");
	return (ret);
}

static long
syscall1(long num, long a1)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1)
	    : "rcx", "r11", "memory");
	return (ret);
}

static long
syscall3(long num, long a1, long a2, long a3)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1), "S"(a2), "d"(a3)
	    : "rcx", "r11", "memory");
	return (ret);
}

static long
proc_copy(void)
{
	return (syscall0(CALL_PROC_COPY));
}

static long
proc_setsid(void)
{
	return (syscall0(CALL_PROC_SETSID));
}

static unsigned long
strlen_s(const char *s)
{
	unsigned long	n;

	n = 0;
	while (s[n]) {
		n++;
	}
	return (n);
}

static void
print(const char *s)
{
	syscall3(CALL_TERM_WRITE, (long)s, (long)strlen_s(s), 0);
}

static void
exit_now(int code)
{
	syscall1(CALL_PROC_EXIT, code);
	for (;;) {
	}
}

static long
drm_call(u64 op, void *arg)
{
	return (syscall3(CALL_DRM_CALL, (long)op, (long)arg, 0));
}

static int
kqueue_create(void)
{
	return ((int)syscall1(CALL_EVENT_KQUEUE, 0));
}

static int
kevent_wait(int kq, struct kevent *changes, int nchanges,
    struct kevent *events, int nevents, s64 timeout_ms)
{
	struct kevent_args	args;

	args.kq_idx = kq;
	args.changelist = changes;
	args.nchanges = nchanges;
	args.eventlist = events;
	args.nevents = nevents;
	args.timeout_ms = timeout_ms;
	return ((int)syscall3(CALL_EVENT_KEVENT, 0, (long)&args, 0));
}

static int
cursor_shape(int x, int y)
{
	if (x < 0 || y < 0) {
		return (0);
	}
	if (y < 19 && x <= y / 2) {
		return (1);
	}
	if (y >= 13 && y < 29 && x >= 6 && x <= 10) {
		return (1);
	}
	if (y >= 18 && y < 23 && x >= 9 && x <= 17) {
		return (1);
	}
	return (0);
}

static int
cursor_outline(int x, int y)
{
	if (!cursor_shape(x, y)) {
		return (0);
	}
	if (!cursor_shape(x - 1, y) || !cursor_shape(x + 1, y) ||
	    !cursor_shape(x, y - 1) || !cursor_shape(x, y + 1)) {
		return (1);
	}
	return (0);
}

static void
put_cursor_pixel(u32 gem, u32 x, u32 y, u32 color)
{
	struct api_drm_rapi_pixel	p;

	p.handle = gem;
	p.pitch = CURSOR_PITCH;
	p.bpp = 32;
	p.x = x;
	p.y = y;
	p.color = color;
	drm_call(DRM_OP_RAPI_PUT_PIXEL, &p);
}

static void
draw_cursor(u32 gem)
{
	struct api_drm_rapi_rect	r;
	u32			x, y;

	r.handle = gem;
	r.pitch = CURSOR_PITCH;
	r.bpp = 32;
	r.x = 0;
	r.y = 0;
	r.width = CURSOR_W;
	r.height = CURSOR_H;
	r.color = 0x00000000;
	drm_call(DRM_OP_RAPI_CLEAR, &r);

	for (y = 0; y < CURSOR_H; y++) {
		for (x = 0; x < CURSOR_W; x++) {
			if (!cursor_shape((int)x, (int)y)) {
				continue;
			}
			if (cursor_outline((int)x, (int)y)) {
				put_cursor_pixel(gem, x, y, 0xFF000000);
			} else {
				put_cursor_pixel(gem, x, y, 0xFFFFFFFF);
			}
		}
	}
}

static int
commit_cursor(u32 plane, u32 fb, int x, int y)
{
	struct api_drm_atomic_req	reqs[9];
	struct api_drm_atomic_commit	commit;

	reqs[0].obj_id = plane;
	reqs[0].prop_id = DRM_PROP_PLANE_FB_ID;
	reqs[0].value = fb;
	reqs[1].obj_id = plane;
	reqs[1].prop_id = DRM_PROP_PLANE_CRTC_X;
	reqs[1].value = (u64)x;
	reqs[2].obj_id = plane;
	reqs[2].prop_id = DRM_PROP_PLANE_CRTC_Y;
	reqs[2].value = (u64)y;
	reqs[3].obj_id = plane;
	reqs[3].prop_id = DRM_PROP_PLANE_CRTC_W;
	reqs[3].value = CURSOR_W;
	reqs[4].obj_id = plane;
	reqs[4].prop_id = DRM_PROP_PLANE_CRTC_H;
	reqs[4].value = CURSOR_H;
	reqs[5].obj_id = plane;
	reqs[5].prop_id = DRM_PROP_PLANE_SRC_X;
	reqs[5].value = 0;
	reqs[6].obj_id = plane;
	reqs[6].prop_id = DRM_PROP_PLANE_SRC_Y;
	reqs[6].value = 0;
	reqs[7].obj_id = plane;
	reqs[7].prop_id = DRM_PROP_PLANE_SRC_W;
	reqs[7].value = CURSOR_W;
	reqs[8].obj_id = plane;
	reqs[8].prop_id = DRM_PROP_PLANE_SRC_H;
	reqs[8].value = CURSOR_H;

	commit.reqs = reqs;
	commit.count = 9;
	commit.flags = 0;
	return ((int)drm_call(DRM_OP_ATOMIC_COMMIT, &commit));
}

void
_start(long argc, char **argv, char **envp)
{
	struct api_drm_gem_create	gem;
	struct api_drm_fb_create	fb;
	struct api_drm_objects	objects;
	struct api_drm_info	info;
	struct kevent		change;
	struct kevent		events[CURSOR_EVENT_BATCH];
	int			i, kq, n, x, y, dx, dy, max_x, max_y;
	long			pid;

	(void)argc;
	(void)argv;
	(void)envp;

	syscall1(CALL_PERSONALITY, 0);
	pid = proc_copy();
	if (pid < 0) {
		print("cursord: first fork failed\n");
		exit_now(1);
	}
	if (pid > 0) {
		exit_now(0);
	}

	if (proc_setsid() < 0) {
		print("cursord: setsid failed\n");
		exit_now(1);
	}

	pid = proc_copy();
	if (pid < 0) {
		print("cursord: second fork failed\n");
		exit_now(1);
	}
	if (pid > 0) {
		exit_now(0);
	}

	if (drm_call(DRM_OP_INFO, &info) != 0 || !info.available) {
		print("cursord: drm not available\n");
		exit_now(1);
	}
	if (drm_call(DRM_OP_GET_OBJECTS, &objects) != 0 ||
	    objects.cursor_plane_id == 0) {
		print("cursord: cursor plane not available\n");
		exit_now(1);
	}

	gem.size = CURSOR_PITCH * CURSOR_H;
	gem.handle = 0;
	if (drm_call(DRM_OP_GEM_CREATE, &gem) != 0 || gem.handle == 0) {
		print("cursord: gem create failed\n");
		exit_now(1);
	}

	fb.gem_handle = gem.handle;
	fb.width = CURSOR_W;
	fb.height = CURSOR_H;
	fb.pitch = CURSOR_PITCH;
	fb.bpp = 32;
	fb.fb_id = 0;
	if (drm_call(DRM_OP_FB_CREATE, &fb) != 0 || fb.fb_id == 0) {
		print("cursord: fb create failed\n");
		exit_now(1);
	}

	draw_cursor(gem.handle);

	x = (int)(info.width / 2);
	y = (int)(info.height / 2);
	max_x = info.width > 0 ? (int)info.width - 1 : 0;
	max_y = info.height > 0 ? (int)info.height - 1 : 0;
	commit_cursor(objects.cursor_plane_id, fb.fb_id, x, y);

	kq = kqueue_create();
	if (kq < 0) {
		print("cursord: kqueue failed\n");
		exit_now(1);
	}

	change.ident = 0;
	change.filter = EVFILT_MOUSE;
	change.flags = EV_ADD | EV_CLEAR;
	change.fflags = 0;
	change.data = 0;
	change.udata = 0;
	if (kevent_wait(kq, &change, 1, 0, 0, -1) < 0) {
		print("cursord: mouse event attach failed\n");
		exit_now(1);
	}

	print("cursord: started\n");

	for (;;) {
		n = kevent_wait(kq, 0, 0, events, CURSOR_EVENT_BATCH, -1);
		if (n <= 0) {
			continue;
		}
		dx = 0;
		dy = 0;
		for (i = 0; i < n; i++) {
			if (events[i].filter != EVFILT_MOUSE) {
				continue;
			}
			dx += (int)MOUSE_DATA_DX(events[i].data);
			dy += (int)MOUSE_DATA_DY(events[i].data);
		}
		if (dx == 0 && dy == 0) {
			continue;
		}
		x += dx;
		y += dy;
		if (x < 0) {
			x = 0;
		}
		if (y < 0) {
			y = 0;
		}
		if (x > max_x) {
			x = max_x;
		}
		if (y > max_y) {
			y = max_y;
		}
		commit_cursor(objects.cursor_plane_id, fb.fb_id, x, y);
	}
}
