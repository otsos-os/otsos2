#define CALL_TERM_WRITE		0x101
#define CALL_TERM_INFO		0x102
#define CALL_INPUT_READ		0x120
#define CALL_INPUT_FLUSH	0x122
#define CALL_DATA_OPEN		0x200
#define CALL_DATA_CLOSE		0x201
#define CALL_DATA_READ		0x202
#define CALL_DATA_WRITE		0x203
#define CALL_FS_STAT		0x209
#define CALL_PROC_EXIT		0x403
#define CALL_PERSONALITY	0xFFFF
#define API_OPEN_READ		0x0001
#define API_OPEN_WRITE		0x0002
#define API_OPEN_CREATE		0x0040
#define API_OPEN_TRUNC		0x0200
#define API_INPUT_NONBLOCK	0x00000001
#define API_FS_TYPE_REG		1
#define API_FS_TYPE_DIR		2
#define OTS_KEY_A		0x0004
#define OTS_KEY_F		0x0009
#define OTS_KEY_G		0x000a
#define OTS_KEY_O		0x0012
#define OTS_KEY_Q		0x0014
#define OTS_KEY_S		0x0016
#define OTS_KEY_X		0x001b
#define OTS_KEY_Z		0x001d
#define OTS_KEY_ENTER		0x0028
#define OTS_KEY_ESC		0x0029
#define OTS_KEY_BACKSPACE	0x002a
#define OTS_KEY_F1		0x003a
#define OTS_KEY_HOME		0x004a
#define OTS_KEY_PAGEUP		0x004b
#define OTS_KEY_DELETE		0x004c
#define OTS_KEY_END		0x004d
#define OTS_KEY_PAGEDOWN	0x004e
#define OTS_KEY_RIGHT		0x004f
#define OTS_KEY_LEFT		0x0050
#define OTS_KEY_DOWN		0x0051
#define OTS_KEY_UP		0x0052
#define OTS_KEY_KP_ENTER	0x0058
#define OTS_KEY_EVENT_PRESS	0x00000001
#define OTS_MOD_CTRL		0x0000000c
#define MAX_LINES	512
#define LINE_CAP	256
#define MAX_PATH	256
#define SEARCH_CAP	80
typedef unsigned long	u64;
typedef unsigned int	u32;
typedef unsigned short	u16;
typedef unsigned char	u8;
typedef long		s64;

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

static char	lines[MAX_LINES][LINE_CAP];
static char	undo_lines[MAX_LINES][LINE_CAP];
static char	current_path[MAX_PATH];
static char	status_msg[160];
static char	search_text[SEARCH_CAP];
static int	line_count;
static int	undo_line_count;
static int	cx, cy;
static int	rowoff, coloff;
static int	undo_cx, undo_cy, undo_dirty;
static int	screen_rows, screen_cols;
static int	dirty, undo_valid, select_all;
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

static u32
strlen_s(const char *s)
{
	u32	n;

	n = 0;
	while (s[n]) {
		n++;
	}
	return (n);
}

static void
memcpy_s(void *dstp, const void *srcp, u32 n)
{
	char		*dst;
	const char	*src;
	u32		i;

	dst = (char *)dstp;
	src = (const char *)srcp;
	for (i = 0; i < n; i++) {
		dst[i] = src[i];
	}
}

static void
memset_s(void *dstp, int v, u32 n)
{
	char	*dst;
	u32	i;

	dst = (char *)dstp;
	for (i = 0; i < n; i++) {
		dst[i] = (char)v;
	}
}

static void
strcpy_cap(char *dst, const char *src, int cap)
{
	int	i;

	i = 0;
	while (i + 1 < cap && src[i]) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static void
append_text(char *dst, int cap, const char *src)
{
	int	i;
	int	j;

	i = 0;
	while (i < cap && dst[i]) {
		i++;
	}
	j = 0;
	while (i + 1 < cap && src[j]) {
		dst[i++] = src[j++];
	}
	dst[i] = '\0';
}

static void
append_int(char *dst, int cap, int value)
{
	char	tmp[16];
	int	i;
	int	n;
	int	neg;

	i = 0;
	n = value;
	neg = 0;
	if (n == 0) {
		append_text(dst, cap, "0");
		return;
	}
	if (n < 0) {
		neg = 1;
		n = -n;
	}
	while (n > 0 && i < (int)sizeof(tmp)) {
		tmp[i++] = (char)('0' + (n % 10));
		n /= 10;
	}
	if (neg) {
		tmp[i++] = '-';
	}
	while (i > 0) {
		char	c[2];

		c[0] = tmp[--i];
		c[1] = '\0';
		append_text(dst, cap, c);
	}
}

static long
term_write(const void *buf, u32 len)
{
	return (syscall3(CALL_TERM_WRITE, (long)buf, (long)len, 0));
}

static void
print(const char *s)
{
	term_write(s, strlen_s(s));
}

static void
proc_exit(int code)
{
	syscall1(CALL_PROC_EXIT, code);
	for (;;) {
	}
}

static long
input_read(struct api_key_event *ev, u32 count, u32 flags)
{
	return (syscall3(CALL_INPUT_READ, (long)ev, (long)count,
	    (long)flags));
}

static void
input_flush(void)
{
	syscall1(CALL_INPUT_FLUSH, 0);
}

static long
term_info(struct api_term_info *info)
{
	return (syscall1(CALL_TERM_INFO, (long)info));
}

static long
data_open(const char *path, int flags)
{
	return (syscall3(CALL_DATA_OPEN, (long)path, (long)flags, 0));
}

static long
data_close(int fd)
{
	return (syscall1(CALL_DATA_CLOSE, (long)fd));
}

static long
data_read(int fd, void *buf, u32 len)
{
	return (syscall3(CALL_DATA_READ, (long)fd, (long)buf, (long)len));
}

static long
data_write(int fd, const void *buf, u32 len)
{
	return (syscall3(CALL_DATA_WRITE, (long)fd, (long)buf, (long)len));
}

static long
fs_stat(const char *path, struct api_fs_stat *st)
{
	return (syscall3(CALL_FS_STAT, (long)path, (long)st, 0));
}

static int
line_len(int y)
{
	return ((int)strlen_s(lines[y]));
}

static void
set_status(const char *s)
{
	strcpy_cap(status_msg, s, sizeof(status_msg));
}

static void
set_status_path(const char *prefix, const char *path)
{
	status_msg[0] = '\0';
	append_text(status_msg, sizeof(status_msg), prefix);
	append_text(status_msg, sizeof(status_msg), path);
}

static void
editor_reset(void)
{
	memset_s(lines, 0, sizeof(lines));
	memset_s(current_path, 0, sizeof(current_path));
	memset_s(status_msg, 0, sizeof(status_msg));
	memset_s(search_text, 0, sizeof(search_text));
	line_count = 1;
	cx = 0;
	cy = 0;
	rowoff = 0;
	coloff = 0;
	dirty = 0;
	undo_valid = 0;
	select_all = 0;
}

static void
save_undo(void)
{
	int	i;

	for (i = 0; i < MAX_LINES; i++) {
		memcpy_s(undo_lines[i], lines[i], LINE_CAP);
	}
	undo_line_count = line_count;
	undo_cx = cx;
	undo_cy = cy;
	undo_dirty = dirty;
	undo_valid = 1;
}

static void
undo_action(void)
{
	int	i;

	if (!undo_valid) {
		set_status("Nothing to undo");
		return;
	}
	for (i = 0; i < MAX_LINES; i++) {
		memcpy_s(lines[i], undo_lines[i], LINE_CAP);
	}
	line_count = undo_line_count;
	cx = undo_cx;
	cy = undo_cy;
	dirty = undo_dirty;
	select_all = 0;
	set_status("Undo");
}

static void
clear_all_text(void)
{
	memset_s(lines, 0, sizeof(lines));
	line_count = 1;
	cx = 0;
	cy = 0;
	rowoff = 0;
	coloff = 0;
	select_all = 0;
}

static void
ensure_cursor(void)
{
	int	len;
	int	view_rows;
	int	view_cols;

	if (cy < 0) {
		cy = 0;
	}
	if (cy >= line_count) {
		cy = line_count - 1;
	}
	len = line_len(cy);
	if (cx > len) {
		cx = len;
	}
	if (cx < 0) {
		cx = 0;
	}

	view_rows = screen_rows - 2;
	view_cols = screen_cols;
	if (view_rows < 1) {
		view_rows = 1;
	}
	if (view_cols < 1) {
		view_cols = 1;
	}
	if (cy < rowoff) {
		rowoff = cy;
	}
	if (cy >= rowoff + view_rows) {
		rowoff = cy - view_rows + 1;
	}
	if (cx < coloff) {
		coloff = cx;
	}
	if (cx >= coloff + view_cols) {
		coloff = cx - view_cols + 1;
	}
}

static void
refresh_size(void)
{
	struct api_term_info	info;

	if (term_info(&info) == 0 && info.rows > 3 && info.cols > 10) {
		screen_rows = info.rows;
		screen_cols = info.cols;
		return;
	}
	screen_rows = 25;
	screen_cols = 80;
}

static void
move_cursor(u16 key)
{
	int	len;

	select_all = 0;
	len = line_len(cy);
	if (key == OTS_KEY_LEFT) {
		if (cx > 0) {
			cx--;
		} else if (cy > 0) {
			cy--;
			cx = line_len(cy);
		}
	} else if (key == OTS_KEY_RIGHT) {
		if (cx < len) {
			cx++;
		} else if (cy + 1 < line_count) {
			cy++;
			cx = 0;
		}
	} else if (key == OTS_KEY_UP) {
		if (cy > 0) {
			cy--;
		}
	} else if (key == OTS_KEY_DOWN) {
		if (cy + 1 < line_count) {
			cy++;
		}
	} else if (key == OTS_KEY_HOME) {
		cx = 0;
	} else if (key == OTS_KEY_END) {
		cx = len;
	} else if (key == OTS_KEY_PAGEUP) {
		cy -= screen_rows - 2;
	} else if (key == OTS_KEY_PAGEDOWN) {
		cy += screen_rows - 2;
	}
	ensure_cursor();
}

static void
insert_char(char ch)
{
	int	len;
	int	i;

	if (select_all) {
		save_undo();
		clear_all_text();
	} else {
		save_undo();
	}
	len = line_len(cy);
	if (len + 1 >= LINE_CAP) {
		set_status("Line too long");
		return;
	}
	for (i = len; i >= cx; i--) {
		lines[cy][i + 1] = lines[cy][i];
	}
	lines[cy][cx] = ch;
	cx++;
	dirty = 1;
}

static void
insert_newline(void)
{
	int	len;
	int	i;

	if (select_all) {
		save_undo();
		clear_all_text();
		dirty = 1;
		return;
	}
	if (line_count >= MAX_LINES) {
		set_status("Too many lines");
		return;
	}
	save_undo();
	len = line_len(cy);
	for (i = line_count; i > cy + 1; i--) {
		memcpy_s(lines[i], lines[i - 1], LINE_CAP);
	}
	memset_s(lines[cy + 1], 0, LINE_CAP);
	for (i = cx; i < len; i++) {
		lines[cy + 1][i - cx] = lines[cy][i];
	}
	lines[cy][cx] = '\0';
	line_count++;
	cy++;
	cx = 0;
	dirty = 1;
}

static void
backspace_char(void)
{
	int	len;
	int	prev_len;
	int	i;

	if (select_all) {
		save_undo();
		clear_all_text();
		dirty = 1;
		return;
	}
	if (cx == 0 && cy == 0) {
		return;
	}
	save_undo();
	if (cx > 0) {
		len = line_len(cy);
		for (i = cx - 1; i < len; i++) {
			lines[cy][i] = lines[cy][i + 1];
		}
		cx--;
	} else {
		prev_len = line_len(cy - 1);
		len = line_len(cy);
		if (prev_len + len >= LINE_CAP) {
			set_status("Join would make line too long");
			return;
		}
		for (i = 0; i <= len; i++) {
			lines[cy - 1][prev_len + i] = lines[cy][i];
		}
		for (i = cy; i + 1 < line_count; i++) {
			memcpy_s(lines[i], lines[i + 1], LINE_CAP);
		}
		memset_s(lines[line_count - 1], 0, LINE_CAP);
		line_count--;
		cy--;
		cx = prev_len;
	}
	dirty = 1;
}

static void
delete_char(void)
{
	int	len;
	int	next_len;
	int	i;

	if (select_all) {
		save_undo();
		clear_all_text();
		dirty = 1;
		return;
	}
	len = line_len(cy);
	if (cx == len && cy + 1 >= line_count) {
		return;
	}
	save_undo();
	if (cx < len) {
		for (i = cx; i < len; i++) {
			lines[cy][i] = lines[cy][i + 1];
		}
	} else {
		next_len = line_len(cy + 1);
		if (len + next_len >= LINE_CAP) {
			set_status("Join would make line too long");
			return;
		}
		for (i = 0; i <= next_len; i++) {
			lines[cy][len + i] = lines[cy + 1][i];
		}
		for (i = cy + 1; i + 1 < line_count; i++) {
			memcpy_s(lines[i], lines[i + 1], LINE_CAP);
		}
		memset_s(lines[line_count - 1], 0, LINE_CAP);
		line_count--;
	}
	dirty = 1;
}

static void
cut_line(void)
{
	int	i;

	if (select_all) {
		save_undo();
		clear_all_text();
		dirty = 1;
		set_status("Selection deleted");
		return;
	}
	save_undo();
	if (line_count == 1) {
		memset_s(lines[0], 0, LINE_CAP);
		cx = 0;
		cy = 0;
	} else {
		for (i = cy; i + 1 < line_count; i++) {
			memcpy_s(lines[i], lines[i + 1], LINE_CAP);
		}
		memset_s(lines[line_count - 1], 0, LINE_CAP);
		line_count--;
		if (cy >= line_count) {
			cy = line_count - 1;
		}
		cx = 0;
	}
	dirty = 1;
	set_status("Line cut");
}

static int
parent_exists(const char *path)
{
	struct api_fs_stat	st;
	char			parent[MAX_PATH];
	int			last;
	int			i;

	last = -1;
	for (i = 0; path[i] && i < MAX_PATH - 1; i++) {
		if (path[i] == '/') {
			last = i;
		}
	}
	if (last < 0) {
		return (1);
	}
	if (last == 0) {
		parent[0] = '/';
		parent[1] = '\0';
	} else {
		for (i = 0; i < last && i < MAX_PATH - 1; i++) {
			parent[i] = path[i];
		}
		parent[i] = '\0';
	}
	if (fs_stat(parent, &st) != 0) {
		return (0);
	}
	return (st.type == API_FS_TYPE_DIR);
}

static int
append_file_char(char c)
{
	int	len;

	if (c == '\r') {
		return (0);
	}
	if (c == '\n') {
		if (line_count >= MAX_LINES) {
			return (-1);
		}
		line_count++;
		return (0);
	}
	len = line_len(line_count - 1);
	if (len + 1 >= LINE_CAP) {
		return (0);
	}
	lines[line_count - 1][len] = c;
	lines[line_count - 1][len + 1] = '\0';
	return (0);
}

static int
load_file(const char *path)
{
	struct api_fs_stat	st;
	char			buf[256];
	long			fd;
	long			n;
	int			i;
	int			new_file;

	new_file = 0;
	if (fs_stat(path, &st) == 0) {
		if (st.type == API_FS_TYPE_DIR) {
			set_status("Cannot open directory");
			return (-1);
		}
	} else {
		if (!parent_exists(path)) {
			set_status("Parent directory not found");
			return (-1);
		}
		fd = data_open(path, API_OPEN_WRITE | API_OPEN_CREATE);
		if (fd < 0) {
			set_status("Cannot create file");
			return (-1);
		}
		data_close((int)fd);
		new_file = 1;
	}

	editor_reset();
	strcpy_cap(current_path, path, sizeof(current_path));
	fd = data_open(path, API_OPEN_READ);
	if (fd < 0) {
		if (new_file) {
			set_status_path("New file: ", path);
			return (0);
		}
		set_status("Cannot open file");
		return (-1);
	}

	line_count = 1;
	for (;;) {
		n = data_read((int)fd, buf, sizeof(buf));
		if (n < 0) {
			data_close((int)fd);
			set_status("Read error");
			return (-1);
		}
		if (n == 0) {
			break;
		}
		for (i = 0; i < n; i++) {
			if (append_file_char(buf[i]) != 0) {
				set_status("File truncated in editor buffer");
				break;
			}
		}
	}
	data_close((int)fd);
	cx = 0;
	cy = 0;
	dirty = 0;
	set_status_path(new_file ? "New file: " : "Opened: ", path);
	return (0);
}

static int
save_file(void)
{
	long	fd;
	long	n;
	int	i;

	if (!current_path[0]) {
		set_status("No file name");
		return (-1);
	}
	fd = data_open(current_path,
	    API_OPEN_WRITE | API_OPEN_CREATE | API_OPEN_TRUNC);
	if (fd < 0) {
		set_status("Save failed: open");
		return (-1);
	}
	for (i = 0; i < line_count; i++) {
		n = data_write((int)fd, lines[i], strlen_s(lines[i]));
		if (n < 0) {
			data_close((int)fd);
			set_status("Save failed: write");
			return (-1);
		}
		if (i + 1 < line_count) {
			n = data_write((int)fd, "\n", 1);
			if (n < 0) {
				data_close((int)fd);
				set_status("Save failed: newline");
				return (-1);
			}
		}
	}
	data_close((int)fd);
	dirty = 0;
	set_status_path("Saved: ", current_path);
	return (0);
}

static int
match_at(int y, int x, const char *needle)
{
	int	i;

	i = 0;
	while (needle[i]) {
		if (!lines[y][x + i] || lines[y][x + i] != needle[i]) {
			return (0);
		}
		i++;
	}
	return (1);
}

static int
find_text(const char *needle)
{
	int	y;
	int	x;
	int	start_y;

	if (!needle[0]) {
		return (0);
	}
	start_y = cy;
	for (y = start_y; y < line_count; y++) {
		for (x = (y == start_y ? cx + 1 : 0); lines[y][x]; x++) {
			if (match_at(y, x, needle)) {
				cy = y;
				cx = x;
				set_status("Found");
				return (1);
			}
		}
	}
	for (y = 0; y <= start_y; y++) {
		for (x = 0; lines[y][x]; x++) {
			if (match_at(y, x, needle)) {
				cy = y;
				cx = x;
				set_status("Found");
				return (1);
			}
		}
	}
	set_status("Not found");
	return (0);
}

static void
goto_line(int line)
{
	if (line < 1) {
		line = 1;
	}
	if (line > line_count) {
		line = line_count;
	}
	cy = line - 1;
	cx = 0;
	set_status("Jumped");
}

static void
write_repeat(char c, int count)
{
	char	buf[80];
	int	i;
	int	n;

	while (count > 0) {
		n = count;
		if (n > (int)sizeof(buf)) {
			n = sizeof(buf);
		}
		for (i = 0; i < n; i++) {
			buf[i] = c;
		}
		term_write(buf, (u32)n);
		count -= n;
	}
}

static void
move_to(int row, int col)
{
	char	buf[32];

	buf[0] = '\0';
	append_text(buf, sizeof(buf), "\033[");
	append_int(buf, sizeof(buf), row);
	append_text(buf, sizeof(buf), ";");
	append_int(buf, sizeof(buf), col);
	append_text(buf, sizeof(buf), "H");
	print(buf);
}

static void
draw_status(void)
{
	char	buf[256];
	int	cols;
	int	len;

	cols = screen_cols > 1 ? screen_cols - 1 : screen_cols;
	move_to(screen_rows - 1, 1);
	print("\033[36m");
	buf[0] = '\0';
	append_text(buf, sizeof(buf), " oven | ");
	append_text(buf, sizeof(buf), current_path[0] ? current_path : "[no file]");
	append_text(buf, sizeof(buf), dirty ? " [+] " : " ");
	append_text(buf, sizeof(buf), "| ");
	append_int(buf, sizeof(buf), cy + 1);
	append_text(buf, sizeof(buf), ":");
	append_int(buf, sizeof(buf), cx + 1);
	append_text(buf, sizeof(buf), select_all ? " | ALL" : "");
	len = strlen_s(buf);
	if (len > cols) {
		len = cols;
	}
	term_write(buf, (u32)len);
	if (len < cols) {
		write_repeat(' ', cols - len);
	}
	print("\033[0m");

	move_to(screen_rows, 1);
	print("\033[33m");
	len = strlen_s(status_msg);
	if (len > cols) {
		len = cols;
	}
	term_write(status_msg, (u32)len);
	if (len < cols) {
		write_repeat(' ', cols - len);
	}
	print("\033[0m");
}

static void
draw_rows(void)
{
	char	buf[LINE_CAP];
	int	view_rows;
	int	y;
	int	file_y;
	int	x;
	int	n;
	int	len;
	int	cols;
	char	c;

	view_rows = screen_rows - 2;
	cols = screen_cols > 1 ? screen_cols - 1 : screen_cols;
	for (y = 0; y < view_rows; y++) {
		file_y = rowoff + y;
		move_to(y + 1, 1);
		print("\033[0m\033[K");
		if (file_y >= line_count) {
			print("\033[90m~\033[0m");
			continue;
		}
		if (select_all) {
			print("\033[33m");
		}
		len = line_len(file_y);
		n = 0;
		for (x = coloff; x < len && n < cols &&
		    n < (int)sizeof(buf); x++) {
			buf[n++] = lines[file_y][x];
		}
		if (n > 0) {
			term_write(buf, (u32)n);
		}
		if (select_all) {
			print("\033[0m");
		}
		if (file_y == cy && cx >= coloff &&
		    cx < coloff + cols) {
			move_to(y + 1, cx - coloff + 1);
			print("\033[92m");
			c = cx < len ? lines[file_y][cx] : '_';
			term_write(&c, 1);
			print("\033[0m");
		}
	}
}

static void
refresh_screen(void)
{
	refresh_size();
	ensure_cursor();
	print("\033[H");
	draw_rows();
	draw_status();
	move_to(cy - rowoff + 1, cx - coloff + 1);
}

static int
read_key(struct api_key_event *ev)
{
	long	n;

	for (;;) {
		n = input_read(ev, 1, 0);
		if (n == 1 && (ev->flags & OTS_KEY_EVENT_PRESS)) {
			return (1);
		}
	}
}

static int
prompt(char *out, int cap, const char *label)
{
	struct api_key_event	ev;
	int			len;

	len = strlen_s(out);
	for (;;) {
		status_msg[0] = '\0';
		append_text(status_msg, sizeof(status_msg), label);
		append_text(status_msg, sizeof(status_msg), out);
		refresh_screen();
		read_key(&ev);
		if (ev.key == OTS_KEY_ESC) {
			set_status("Cancelled");
			return (0);
		}
		if (ev.key == OTS_KEY_ENTER || ev.key == OTS_KEY_KP_ENTER) {
			return (1);
		}
		if (ev.key == OTS_KEY_BACKSPACE) {
			if (len > 0) {
				out[--len] = '\0';
			}
			continue;
		}
		if (ev.ch >= 32 && ev.ch < 127 && len + 1 < cap) {
			out[len++] = (char)ev.ch;
			out[len] = '\0';
		}
	}
}

static void
open_prompt(void)
{
	char	path[MAX_PATH];

	path[0] = '\0';
	if (!prompt(path, sizeof(path), "Open: ")) {
		return;
	}
	if (!path[0]) {
		set_status("Empty path");
		return;
	}
	load_file(path);
}

static void
save_prompt_or_file(void)
{
	char	path[MAX_PATH];

	if (!current_path[0]) {
		path[0] = '\0';
		if (!prompt(path, sizeof(path), "Save as: ")) {
			return;
		}
		if (!path[0]) {
			set_status("Empty path");
			return;
		}
		strcpy_cap(current_path, path, sizeof(current_path));
	}
	save_file();
}

static void
find_prompt(void)
{
	if (!prompt(search_text, sizeof(search_text), "Find: ")) {
		return;
	}
	find_text(search_text);
}

static int
atoi_s(const char *s)
{
	int	n;
	int	i;

	n = 0;
	i = 0;
	while (s[i] >= '0' && s[i] <= '9') {
		n = n * 10 + (s[i] - '0');
		i++;
	}
	return (n);
}

static void
goto_prompt(void)
{
	char	buf[16];

	buf[0] = '\0';
	if (!prompt(buf, sizeof(buf), "Goto line: ")) {
		return;
	}
	goto_line(atoi_s(buf));
}

static void
help_screen(void)
{
	struct api_key_event	ev;

	print("\033[2J\033[H");
	print("\033[36moven help\033[0m\n\n");
	print("Ctrl+S  save\n");
	print("Ctrl+Q  quit\n");
	print("Ctrl+O  open file\n");
	print("Ctrl+X  cut current line\n");
	print("Ctrl+Z  undo\n");
	print("Ctrl+A  select all\n");
	print("Ctrl+F  find\n");
	print("Ctrl+G  goto line\n");
	print("F1      help\n\n");
	print("Arrows/Home/End/PageUp/PageDown move cursor.\n");
	print("Esc cancels prompts. Press any key to return.");
	read_key(&ev);
	set_status("Help closed");
}

static void
handle_ctrl(u16 key)
{
	if (key == OTS_KEY_S) {
		save_prompt_or_file();
	} else if (key == OTS_KEY_Q) {
		input_flush();
		print("\033[2J\033[H");
		proc_exit(0);
	} else if (key == OTS_KEY_O) {
		open_prompt();
	} else if (key == OTS_KEY_X) {
		cut_line();
	} else if (key == OTS_KEY_Z) {
		undo_action();
	} else if (key == OTS_KEY_A) {
		select_all = 1;
		set_status("All selected");
	} else if (key == OTS_KEY_F) {
		find_prompt();
	} else if (key == OTS_KEY_G) {
		goto_prompt();
	}
}

static void
process_key(struct api_key_event *ev)
{
	if (ev->mods & OTS_MOD_CTRL) {
		handle_ctrl(ev->key);
		return;
	}
	if (ev->key == OTS_KEY_F1) {
		help_screen();
	} else if (ev->key == OTS_KEY_LEFT || ev->key == OTS_KEY_RIGHT ||
	    ev->key == OTS_KEY_UP || ev->key == OTS_KEY_DOWN ||
	    ev->key == OTS_KEY_HOME || ev->key == OTS_KEY_END ||
	    ev->key == OTS_KEY_PAGEUP || ev->key == OTS_KEY_PAGEDOWN) {
		move_cursor(ev->key);
	} else if (ev->key == OTS_KEY_BACKSPACE) {
		backspace_char();
	} else if (ev->key == OTS_KEY_DELETE) {
		delete_char();
	} else if (ev->key == OTS_KEY_ENTER || ev->key == OTS_KEY_KP_ENTER) {
		insert_newline();
	} else if (ev->ch >= 32 && ev->ch < 127) {
		insert_char((char)ev->ch);
	}
	ensure_cursor();
}

void
_start(long argc, char **argv, char **envp)
{
	struct api_key_event	ev;

	(void)envp;
	syscall1(CALL_PERSONALITY, 0);
	editor_reset();
	input_flush();
	if (argc > 1 && argv && argv[1]) {
		load_file(argv[1]);
	} else {
		set_status("Ctrl+O open, Ctrl+S save, F1 help");
	}

	for (;;) {
		refresh_screen();
		read_key(&ev);
		process_key(&ev);
	}
}
