/*
 * sh - otsos2 userspace shell
 *
 * Built-in commands: echo, pwd, cd, ls, ps, cpus, cat, clear, exit, help, env
 * External commands: spawn + wait via CALL_PROC_SPAWN/CALL_PROC_WAIT
 * PATH resolution: /bin/<cmd> for external binaries
 *
 * Freestanding: no libc, raw syscalls via inline asm.
 * (deepseek code)
 */

#define CALL_TERM_READ  0x100
#define CALL_TERM_WRITE 0x101
#define TERM_READ_IGNORE_SIGINT 0x00000001
#define CALL_DATA_OPEN  0x200
#define CALL_DATA_CLOSE 0x201
#define CALL_DATA_READ  0x202
#define CALL_FS_CHDIR   0x206
#define CALL_FS_GETCWD  0x207
#define CALL_FS_LISTDIR 0x208
#define CALL_PROC_SPAWN 0x402
#define CALL_PROC_EXIT  0x403
#define CALL_PROC_WAIT  0x404
#define CALL_PROC_LIST  0x406
#define CALL_KUSR_AUTH  0x407
#define CALL_SYS_INFO   0x500
#define CALL_SYS_MEMINFO 0x501
#define CALL_SYS_KMEMINFO 0x502
#define CALL_SYS_TIMEINFO 0x504
#define CALL_SYS_CPUINFO 0x506
#define CALL_DRM_CALL    0x600

#define DRM_OP_INFO            1
#define DRM_OP_DRIVER_LIST     16
#define DRM_OP_DRIVER_SWITCH   15

#define CALL_PERSONALITY 0xFFFF

#define API_OPEN_READ   0x0001

/* Kernel error codes (mirror of kernel/api/errno.h).
 * Syscalls return these as negative values on failure. */
#define ERR_PERM          1
#define ERR_NOT_FOUND     2
#define ERR_NO_PROC       3
#define ERR_INTR          4
#define ERR_IO            5
#define ERR_NO_DEVICE_ADDR 6
#define ERR_TOO_BIG       7
#define ERR_BAD_IMAGE     8
#define ERR_BAD_HANDLE    9
#define ERR_NO_CHILD      10
#define ERR_RETRY         11
#define ERR_NO_MEMORY     12
#define ERR_ACCESS        13
#define ERR_BAD_ADDR      14
#define ERR_BUSY          16
#define ERR_EXISTS        17
#define ERR_CROSS_DEVICE  18
#define ERR_NO_DEVICE     19
#define ERR_NOT_DIR       20
#define ERR_IS_DIR        21
#define ERR_BAD_VALUE     22
#define ERR_OBJECTS_FULL  23
#define ERR_HANDLES_FULL  24
#define ERR_NOT_TERM      25
#define ERR_FILE_TOO_BIG  27
#define ERR_NO_SPACE      28
#define ERR_NOT_SEEKABLE  29
#define ERR_READ_ONLY     30
#define ERR_PIPE_CLOSED   32
#define ERR_NO_CALL       38
#define ERR_NOT_SUPPORTED 95

#define MAX_LINE  512
#define MAX_ARGS  64
#define MAX_PATH  256
#define MAX_DIRENT 128
#define MAX_PROCS 64
#define MAX_CPUS 32
#define MAX_CPU_PIDS 64

typedef unsigned long u64;
typedef unsigned int  u32;
typedef unsigned char u8;
char* NULL = (char*)0;
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

struct api_cpu_entry {
  u32 cpu_index;
  u32 lapic_id;
  u32 present;
  u32 online;
  u32 pid;
  u32 tid;
  u32 state;
  u32 pid_count;
  u32 pids[MAX_CPU_PIDS];
  char proc_name[32];
};

struct api_cpuinfo {
  u32 cpu_count;
  u32 entry_count;
  struct api_cpu_entry entries[MAX_CPUS];
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
  long timezone_offset;
  char clocksource[32];
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

static char **g_envp;
static int g_kusr_authed = 0;
/* ------------------------------------------------------------------ */
/*  raw syscall wrappers                                              */
/* ------------------------------------------------------------------ */

static long syscall1(long num, long a1) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(num), "D"(a1)
                   : "rcx", "r11", "memory");
  return ret;
}

static long syscall3(long num, long a1, long a2, long a3) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(num), "D"(a1), "S"(a2), "d"(a3)
                   : "rcx", "r11", "memory");
  return ret;
}

static long term_write(const void *buf, u32 len) {
  return syscall3(CALL_TERM_WRITE, (long)buf, (long)len, 0);
}

static long term_read(void *buf, u32 len) {
  return syscall3(CALL_TERM_READ, (long)buf, (long)len,
                  TERM_READ_IGNORE_SIGINT);
}

static long fs_chdir(const char *path) {
  return syscall3(CALL_FS_CHDIR, (long)path, 0, 0);
}

static long fs_getcwd(char *buf, u32 size) {
  return syscall3(CALL_FS_GETCWD, (long)buf, (long)size, 0);
}

static long fs_listdir(const char *path, struct api_dirent *buf, u32 max) {
  return syscall3(CALL_FS_LISTDIR, (long)path, (long)buf, (long)max);
}

static long proc_spawn(const char *path, const char *const *argv,
                       const char *const *envp) {
  return syscall3(CALL_PROC_SPAWN, (long)path, (long)argv, (long)envp);
}

static long proc_wait(int *status) {
  return syscall3(CALL_PROC_WAIT, (long)status, 0, 0);
}

static long proc_list(struct api_proc_info *buf, u32 max) {
  return syscall3(CALL_PROC_LIST, (long)buf, (long)max, 0);
}

static long kusr_auth(const char *password) {
  return syscall3(CALL_KUSR_AUTH, (long)password, 0, 0);
}

static long sys_meminfo(struct api_meminfo *buf) {
  return syscall1(CALL_SYS_MEMINFO, (long)buf);
}

static long sys_kmeminfo(struct api_kmeminfo *buf) {
  return syscall1(CALL_SYS_KMEMINFO, (long)buf);
}

static long sys_cpuinfo(struct api_cpuinfo *buf) {
  return syscall1(CALL_SYS_CPUINFO, (long)buf);
}

static long sys_timeinfo(struct api_timeinfo *buf) {
  return syscall1(CALL_SYS_TIMEINFO, (long)buf);
}

static long drm_call(u64 op, void *arg) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"((long)CALL_DRM_CALL), "D"((long)op), "S"((long)arg)
                   : "rcx", "r11", "memory");
  return ret;
}

static long data_open(const char *path, int flags) {
  return syscall3(CALL_DATA_OPEN, (long)path, (long)flags, 0);
}

static long data_close(int handle) {
  return syscall3(CALL_DATA_CLOSE, (long)handle, 0, 0);
}

static long data_read(int handle, void *buf, u32 len) {
  return syscall3(CALL_DATA_READ, (long)handle, (long)buf, (long)len);
}

static void proc_exit(int code) {
  syscall1(CALL_PROC_EXIT, code);
  for (;;) {}
}

/* ------------------------------------------------------------------ */
/*  string utilities (no libc)                                        */
/* ------------------------------------------------------------------ */

static u32 strlen_s(const char *s) {
  u32 n = 0;
  while (s[n]) n++;
  return n;
}

static int strcmp_s(const char *a, const char *b) {
  while (*a && *b && *a == *b) { a++; b++; }
  return (unsigned char)*a - (unsigned char)*b;
}



static void memcpy_s(void *d, const void *s, u32 n) {
  char *dst = (char *)d;
  const char *src = (const char *)s;
  for (u32 i = 0; i < n; i++) dst[i] = src[i];
}

static void memset_s(void *d, int v, u32 n) {
  char *dst = (char *)d;
  for (u32 i = 0; i < n; i++) dst[i] = (char)v;
}


/* ------------------------------------------------------------------ */
/*  number formatting                                                 */
/* ------------------------------------------------------------------ */

static int itoa_s(int val, char *out) {
  int pos = 0;
  if (val == 0) {
    out[0] = '0'; out[1] = '\0';
    return 1;
  }
  if (val < 0) {
    out[pos++] = '-';
    val = -val;
  }
  char tmp[12];
  int t = 0;
  while (val > 0) {
    tmp[t++] = '0' + (val % 10);
    val /= 10;
  }
  while (t > 0) out[pos++] = tmp[--t];
  out[pos] = '\0';
  return pos;
}

/* ------------------------------------------------------------------ */
/*  output helpers                                                    */
/* ------------------------------------------------------------------ */

static void print(const char *s) {
  term_write(s, strlen_s(s));
}

static void println(const char *s) {
  print(s);
  print("\n");
}

static void print_int(int v) {
  char buf[12];
  itoa_s(v, buf);
  print(buf);
}

static void printc(char c) {
  term_write(&c, 1);
}

/* ------------------------------------------------------------------ */
/*  error reporting                                                   */
/* ------------------------------------------------------------------ */

/* Convert a (possibly negative) syscall return value into a short
 * human-readable error string. Treats `code` as an error code by its
 * absolute value. */
static const char *err_str(long code) {
  if (code < 0) code = -code;
  switch (code) {
  case 0:              return "ok";
  case ERR_PERM:       return "operation not permitted";
  case ERR_NOT_FOUND:  return "no such file or directory";
  case ERR_NO_PROC:    return "no such process";
  case ERR_INTR:       return "interrupted";
  case ERR_IO:         return "i/o error";
  case ERR_NO_DEVICE_ADDR: return "no device address";
  case ERR_TOO_BIG:    return "argument/value too large";
  case ERR_BAD_IMAGE:  return "invalid executable image";
  case ERR_BAD_HANDLE: return "bad file handle";
  case ERR_NO_CHILD:   return "no child process";
  case ERR_RETRY:      return "resource busy, try again";
  case ERR_NO_MEMORY:  return "out of memory";
  case ERR_ACCESS:     return "permission denied";
  case ERR_BAD_ADDR:   return "bad address";
  case ERR_BUSY:       return "device or resource busy";
  case ERR_EXISTS:     return "file exists";
  case ERR_CROSS_DEVICE: return "cross-device link";
  case ERR_NO_DEVICE:  return "no such device";
  case ERR_NOT_DIR:    return "not a directory";
  case ERR_IS_DIR:     return "is a directory";
  case ERR_BAD_VALUE:  return "invalid argument";
  case ERR_OBJECTS_FULL: return "kernel object table full";
  case ERR_HANDLES_FULL: return "process handle table full";
  case ERR_NOT_TERM:   return "not a terminal";
  case ERR_FILE_TOO_BIG: return "file too large";
  case ERR_NO_SPACE:   return "no space left on device";
  case ERR_NOT_SEEKABLE: return "not seekable";
  case ERR_READ_ONLY:  return "read-only filesystem";
  case ERR_PIPE_CLOSED: return "broken pipe";
  case ERR_NO_CALL:    return "no such syscall";
  case ERR_NOT_SUPPORTED: return "operation not supported";
  default:             return "unknown error";
  }
}

/* Print "<prefix>: <reason> (code N)\n".
 * `ret` is the raw syscall return value (negative on error). */
static void print_err(const char *prefix, long ret) {
  print(prefix);
  print(": ");
  print(err_str(ret));
  print(" (code ");
  print_int((int)ret);
  print(")\n");
}

/* ------------------------------------------------------------------ */
/*  line input                                                        */
/* ------------------------------------------------------------------ */

static int read_line(char *buf, int max) {
  int pos = 0;
  for (;;) {
    char c;
    long n = term_read(&c, 1);
    if (n <= 0) continue;
    if (c == '\r' || c == '\n') {
      printc('\n');
      buf[pos] = '\0';
      return pos;
    }
    if (c == '\b' || c == 0x7F) {
      if (pos > 0) {
        pos--;
        print("\b \b");
      }
      continue;
    }
    if (c == 0x03) {
      print("^C\n");
      buf[0] = '\0';
      return 0;
    }
    if (c >= 32 && c < 127 && pos < max - 1) {
      buf[pos++] = c;
      printc(c);
    }
  }
}

/* ------------------------------------------------------------------ */
/*  command parser                                                    */
/* ------------------------------------------------------------------ */

static int parse_line(char *line, char **argv, int max_args) {
  int argc = 0;
  char *p = line;

  while (*p == ' ' || *p == '\t') p++;

  while (*p && argc < max_args - 1) {
    if (*p == '"' || *p == '\'') {
      char quote = *p;
      p++;
      argv[argc++] = p;
      while (*p && *p != quote) p++;
      if (*p == quote) {
        *p = '\0';
        p++;
      }
    } else {
      argv[argc++] = p;
      while (*p && *p != ' ' && *p != '\t') p++;
      if (*p) {
        *p = '\0';
        p++;
      }
    }
    while (*p == ' ' || *p == '\t') p++;
  }
  argv[argc] = NULL;
  return argc;
}

/* ------------------------------------------------------------------ */
/*  external command helpers                                          */
/* ------------------------------------------------------------------ */

/* Try to open `path` for reading. Returns 1 if it exists, 0 if it does
 * not, or a negative error code if the open failed for another reason. */
static long file_exists(const char *path) {
  int fd = data_open(path, API_OPEN_READ);
  if (fd >= 0) {
    data_close(fd);
    return 1;
  }
  if (fd == -ERR_NOT_FOUND) return 0;
  return fd; /* propagate real error */
}

/* Resolve a command name to a filesystem path.
 * Returns 0 on success (path filled in `out`), -ERR_NOT_FOUND if the
 * command does not exist, or another negative error code. */
static long resolve_path(const char *cmd, char *out, u32 out_sz) {
  if (cmd[0] == '/' || cmd[0] == '.') {
    u32 len = strlen_s(cmd);
    if (len >= out_sz) return -ERR_TOO_BIG;
    memcpy_s(out, cmd, len + 1);
    return file_exists(out) ? 0 : -ERR_NOT_FOUND;
  }

  const char *prefix = "/bin/";
  u32 plen = strlen_s(prefix);
  u32 clen = strlen_s(cmd);
  if (plen + clen >= out_sz) return -ERR_TOO_BIG;
  memcpy_s(out, prefix, plen);
  memcpy_s(out + plen, cmd, clen + 1);
  return file_exists(out) ? 0 : -ERR_NOT_FOUND;
}

static int run_external(const char *path, char **argv, char **envp) {
  long pid = proc_spawn(path, (const char *const *)argv,
                        (const char *const *)envp);
  if (pid < 0) {
    print("sh: ");
    print(path);
    print(": ");
    print(err_str(pid));
    print(" (code ");
    print_int((int)pid);
    print(")\n");
    return -1;
  }

  int status;
  for (;;) {
    long w = proc_wait(&status);
    if (w > 0) break;
    if (w < 0 && w != -ERR_NO_CHILD) {
      print_err("sh: wait", w);
      return -1;
    }
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/*  built-in: drm_list                                                 */
/* ------------------------------------------------------------------ */

static void cmd_drm_list(void) {
  struct api_drm_driver_entry entries[8];
  struct api_drm_driver_list list;
  list.entries = entries;
  list.max_entries = 8;
  list.count = 0;

  long ret = drm_call(DRM_OP_DRIVER_LIST, &list);
  if (ret < 0) {
    print_err("drm_list", ret);
    return;
  }

  println("ID  NAME                 STATUS");
  for (u32 i = 0; i < list.count; i++) {
    print_int((int)entries[i].id);
    print("  ");
    print(entries[i].name);
    if (entries[i].active) {
      print("  [active]");
    }
    printc('\n');
  }

  if (list.count == 0) {
    println("(no drivers registered)");
  }
}

/* ------------------------------------------------------------------ */
/*  built-in: drm_switch                                               */
/* ------------------------------------------------------------------ */

static int parse_nonneg(const char *s) {
  int v = 0;
  for (int i = 0; s[i]; i++) {
    if (s[i] < '0' || s[i] > '9') return -1;
    v = v * 10 + (s[i] - '0');
  }
  return v;
}

static void cmd_drm_switch(int argc, char **argv) {
  if (argc < 2) {
    println("drm_switch: usage: drm_switch <id>");
    println("  use drm_list to see available drivers");
    return;
  }

  int id = parse_nonneg(argv[1]);
  if (id < 0) {
    println("drm_switch: invalid id");
    return;
  }

  struct api_drm_driver_switch sw;
  sw.id = (u32)id;

  long ret = drm_call(DRM_OP_DRIVER_SWITCH, &sw);
  if (ret < 0) {
    if (ret == -ERR_PERM) {
      println("drm_switch: permission denied (need kusr)");
    } else {
      print_err("drm_switch", ret);
    }
    return;
  }

  println("drm_switch: ok");
}

/* ------------------------------------------------------------------ */
/*  built-in: help                                                    */
/* ------------------------------------------------------------------ */

static void cmd_help(void) {
  println("otsos2 sh - built-in commands:");
  println("  echo [text...]     print text");
  println("  pwd                print working directory");
  println("  cd <path>          change directory");
  println("  ls [path]          list directory");
  println("  ps                 list processes");
  println("  cpus               list CPUs and running PIDs");
  println("  time               show time info");
  println("  mem                show memory info");
  println("  cat <file>         print file contents");
  println("  clear              clear screen");
  println("  color <hex>        set text color (e.g. FF0000)");
  println("  kusr               authenticate as kernel user");
  println("  drm_list           list DRM drivers");
  println("  drm_switch <id>    switch DRM driver (kusr only)");
  println("  env                print environment");
  println("  exit               exit shell");
  println("  help               this help");
  println("External commands: /bin/<name> or absolute paths");
}

/* ------------------------------------------------------------------ */
/*  built-in: echo                                                    */
/* ------------------------------------------------------------------ */

static void cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    print(argv[i]);
    if (i + 1 < argc) printc(' ');
  }
  printc('\n');
}

/* ------------------------------------------------------------------ */
/*  built-in: pwd                                                     */
/* ------------------------------------------------------------------ */

static void cmd_pwd(void) {
  char buf[MAX_PATH];
  long ret = fs_getcwd(buf, sizeof(buf));
  if (ret < 0) {
    print_err("pwd", ret);
  } else {
    println(buf);
  }
}

/* ------------------------------------------------------------------ */
/*  built-in: cd                                                      */
/* ------------------------------------------------------------------ */

static void cmd_cd(int argc, char **argv) {
  const char *path;
  if (argc < 2) {
    path = "/";
  } else {
    path = argv[1];
  }
  long ret = fs_chdir(path);
  if (ret < 0) {
    print("cd: ");
    print(path);
    print(": ");
    print(err_str(ret));
    print(" (code ");
    print_int((int)ret);
    print(")\n");
  }
}

/* ------------------------------------------------------------------ */
/*  built-in: ls                                                      */
/* ------------------------------------------------------------------ */

static void cmd_ls(int argc, char **argv) {
  const char *path = (argc > 1) ? argv[1] : "";
  struct api_dirent entries[MAX_DIRENT];
  long n = fs_listdir(path, entries, MAX_DIRENT);
  if (n < 0) {
    print("ls: ");
    if (path[0]) {
      print(path);
      print(": ");
    }
    print(err_str(n));
    print(" (code ");
    print_int((int)n);
    print(")\n");
    return;
  }
  for (long i = 0; i < n; i++) {
    print(entries[i].name);
    if (entries[i].type == 1) printc('/');
    print("  ");
  }
  printc('\n');
}

/* ------------------------------------------------------------------ */
/*  built-in: mem                                                      */
/* ------------------------------------------------------------------ */

static void print_u64(u64 v) {
  char tmp[24];
  int t = 0;
  if (v == 0) {
    print("0");
    return;
  }
  while (v > 0) {
    tmp[t++] = '0' + (int)(v % 10);
    v /= 10;
  }
  while (t > 0) printc(tmp[--t]);
}

static void print_padded(int v, int width) {
  char tmp[12];
  int t = 0;
  if (v == 0) {
    tmp[t++] = '0';
  } else {
    while (v > 0) {
      tmp[t++] = '0' + (v % 10);
      v /= 10;
    }
  }
  while (t < width) {
    printc('0');
    t++;
  }
  while (t > 0) printc(tmp[--t]);
}

static int is_leap(int y) {
  return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

static u32 days_in_month_int(int y, int m) {
  static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (m == 2 && is_leap(y)) return 29;
  return days[m - 1];
}

static void print_datetime(u64 sec) {
  u64 days;
  int sec_of_day;
  int hour, min, s;
  int year, month, day;
  int rem;

  days = sec / 86400ULL;
  sec_of_day = (int)(sec % 86400ULL);
  hour = sec_of_day / 3600;
  rem = sec_of_day % 3600;
  min = rem / 60;
  s = rem % 60;

  year = 1970;
  while (1) {
    int ydays = is_leap(year) ? 366 : 365;
    if ((int)days < ydays) break;
    days -= ydays;
    year++;
  }

  month = 1;
  while (1) {
    int mdays = days_in_month_int(year, month);
    if ((int)days < mdays) break;
    days -= mdays;
    month++;
  }
  day = (int)days + 1;

  print_padded(year, 4);
  printc('-');
  print_padded(month, 2);
  printc('-');
  print_padded(day, 2);
  printc(' ');
  print_padded(hour, 2);
  printc(':');
  print_padded(min, 2);
  printc(':');
  print_padded(s, 2);
}

static void print_duration(u64 sec) {
  int s, m, h, d;
  int rem;

  d = (int)(sec / 86400ULL);
  rem = (int)(sec % 86400ULL);
  h = rem / 3600;
  rem = rem % 3600;
  m = rem / 60;
  s = rem % 60;

  if (d > 0) {
    print_int(d);
    print("d ");
  }
  print_padded(h, 2);
  printc(':');
  print_padded(m, 2);
  printc(':');
  print_padded(s, 2);
}

static void cmd_mem(void) {
  struct api_meminfo mi;
  long ret = sys_meminfo(&mi);
  if (ret < 0) {
    print_err("mem", ret);
    return;
  }

  println("=== Memory Info ===");

  print("RAM total      : ");
  print_u64(mi.ram_total_kb);
  println(" KB");

  print("RAM free       : ");
  print_u64(mi.ram_free_kb);
  println(" KB");

  println("");
  print("Pages total    : ");
  print_u64(mi.pages_total);
  printc('\n');

  print("Pages free     : ");
  print_u64(mi.pages_free);
  printc('\n');

  print("Pages active   : ");
  print_u64(mi.pages_active);
  printc('\n');

  print("Pages inactive : ");
  print_u64(mi.pages_inactive);
  printc('\n');

  print("Pages cache    : ");
  print_u64(mi.pages_cache);
  printc('\n');

  print("Pages wired    : ");
  print_u64(mi.pages_wired);
  printc('\n');

  println("");
  print("User mmap base : 0x");
  {
    u64 v = mi.mmap_base;
    char hex[17];
    int h = 0;
    if (v == 0) { hex[h++] = '0'; }
    while (v > 0) { int d = (int)(v % 16); hex[h++] = d < 10 ? '0'+d : 'A'+d-10; v /= 16; }
    while (h > 0) printc(hex[--h]);
  }
  printc('\n');

  print("User mmap limit: 0x");
  {
    u64 v = mi.mmap_limit;
    char hex[17];
    int h = 0;
    if (v == 0) { hex[h++] = '0'; }
    while (v > 0) { int d = (int)(v % 16); hex[h++] = d < 10 ? '0'+d : 'A'+d-10; v /= 16; }
    while (h > 0) printc(hex[--h]);
  }
  printc('\n');

  print("User heap size : ");
  print_u64(mi.user_heap_size_kb / 1024);
  println(" MB");

  if (g_kusr_authed) {
    struct api_kmeminfo ki;
    long kr = sys_kmeminfo(&ki);
    if (kr < 0) {
      println("");
      print("kmem info: permission denied");
      printc('\n');
      return;
    }

    println("");
    println("=== Kernel Memory (kusr) ===");

    print("Kmem heap total: ");
    print_u64(ki.kmem_heap_total_kb);
    println(" KB");

    print("Kmem heap used : ");
    print_u64(ki.kmem_heap_used_kb);
    println(" KB");

    print("Kmem heap free : ");
    print_u64(ki.kmem_heap_free_kb);
    println(" KB");

    print("Bootmem free   : ");
    print_u64(ki.bootmem_free_kb);
    println(" KB");
  }
}

/* ------------------------------------------------------------------ */
/*  built-in: ps                                                       */
/* ------------------------------------------------------------------ */

static const char *state_name(u32 s) {
  switch (s) {
  case 1: return "EMBRYO";
  case 2: return "RUN";
  case 3: return "ACTIVE";
  case 4: return "SLEEP";
  case 5: return "ZOMBIE";
  default: return "FREE";
  }
}

static void cmd_ps(void) {
  struct api_proc_info procs[MAX_PROCS];
  long n = proc_list(procs, MAX_PROCS);
  if (n < 0) {
    print_err("ps", n);
    return;
  }
  println("PID\tPPID\tSTATE\tNAME");
  for (long i = 0; i < n; i++) {
    print_int((int)procs[i].pid);
    printc('\t');
    print_int((int)procs[i].ppid);
    printc('\t');
    print(state_name(procs[i].state));
    printc('\t');
    println(procs[i].name);
  }
}

/* ------------------------------------------------------------------ */
/*  built-in: cpus                                                    */
/* ------------------------------------------------------------------ */

static void cmd_cpus(void) {
  struct api_cpuinfo info;
  long ret = sys_cpuinfo(&info);
  if (ret < 0) {
    print_err("cpus", ret);
    return;
  }

  print("CPUs detected: ");
  print_int((int)info.cpu_count);
  printc('\n');
  println("CPU\tLAPIC\tONLINE\tCURPID\tTID\tSTATE\tNAME\tPIDS");
  for (u32 i = 0; i < info.entry_count && i < MAX_CPUS; i++) {
    struct api_cpu_entry *cpu = &info.entries[i];
    print_int((int)cpu->cpu_index);
    printc('\t');
    print_int((int)cpu->lapic_id);
    printc('\t');
    print(cpu->online ? "yes" : "no");
    printc('\t');
    if (cpu->pid) {
      print_int((int)cpu->pid);
    } else {
      print("-");
    }
    printc('\t');
    if (cpu->tid) {
      print_int((int)cpu->tid);
    } else {
      print("-");
    }
    printc('\t');
    print(state_name(cpu->state));
    printc('\t');
    if (cpu->proc_name[0]) {
      print(cpu->proc_name);
    } else {
      print("-");
    }
    printc('\t');
    if (cpu->pid_count == 0) {
      print("-");
    }
    for (u32 j = 0; j < cpu->pid_count && j < MAX_CPU_PIDS; j++) {
      if (j > 0) {
        printc(',');
      }
      print_int((int)cpu->pids[j]);
    }
    printc('\n');
  }
}

/* ------------------------------------------------------------------ */
/*  built-in: time                                                    */
/* ------------------------------------------------------------------ */

static void cmd_time(void) {
  struct api_timeinfo ti;
  long ret = sys_timeinfo(&ti);
  if (ret < 0) {
    print_err("time", ret);
    return;
  }

  println("=== Time Info ===");
  print("Local date     : ");
  print_datetime(ti.local_sec);
  printc('\n');
  print("Wall-clock date: ");
  print_datetime(ti.wall_sec);
  printc('\n');
  print("Timezone offset: ");
  print_int((int)(ti.timezone_offset / 3600));
  print(" hours\n");
  print("Wall-clock sec : ");
  print_u64(ti.wall_sec);
  printc('\n');
  print("Wall-clock ns  : ");
  print_u64(ti.wall_nsec);
  printc('\n');
  print("Uptime         : ");
  print_duration(ti.uptime_sec);
  printc('\n');
  print("Uptime sec     : ");
  print_u64(ti.uptime_sec);
  printc('\n');
  print("Uptime ns      : ");
  print_u64(ti.uptime_nsec);
  printc('\n');
  print("Timer ticks    : ");
  print_u64(ti.ticks);
  printc('\n');
  print("Timer frequency: ");
  print_u64(ti.frequency);
  printc('\n');
  print("Clocksource    : ");
  println(ti.clocksource);
}

/* ------------------------------------------------------------------ */
/*  built-in: cat                                                     */
/* ------------------------------------------------------------------ */

static void cmd_cat(int argc, char **argv) {
  if (argc < 2) {
    println("cat: missing file");
    return;
  }
  int fd = data_open(argv[1], API_OPEN_READ);
  if (fd < 0) {
    print("cat: ");
    print(argv[1]);
    print(": ");
    print(err_str(fd));
    print(" (code ");
    print_int((int)fd);
    print(")\n");
    return;
  }
  char buf[256];
  for (;;) {
    long n = data_read(fd, buf, sizeof(buf));
    if (n < 0) {
      print("\ncat: ");
      print(argv[1]);
      print(": read error: ");
      print(err_str(n));
      print(" (code ");
      print_int((int)n);
      print(")\n");
      break;
    }
    if (n == 0) break;
    term_write(buf, (u32)n);
  }
  data_close(fd);
}

/* ------------------------------------------------------------------ */
/*  built-in: clear                                                   */
/* ------------------------------------------------------------------ */

static void cmd_clear(void) {
  print("\033[2J\033[H");
}

/* ------------------------------------------------------------------ */
/*  built-in: env                                                     */
/* ------------------------------------------------------------------ */

static void cmd_env(void) {
  if (!g_envp) return;
  for (int i = 0; g_envp[i]; i++) {
    println(g_envp[i]);
  }
}

/* ------------------------------------------------------------------ */
/*  built-in: color                                                   */
/* ------------------------------------------------------------------ */

static int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void cmd_color(int argc, char **argv) {
  if (argc < 2) {
    print("\033[0m\033[39m");
    println("color reset");
    return;
  }

  const char *hex = argv[1];
  if (hex[0] == '#') hex++;
  if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) hex += 2;

  u32 len = strlen_s(hex);
  if (len != 6) {
    println("color: need 6 hex digits (e.g. FF0000)");
    return;
  }

  int r = (hex_digit(hex[0]) << 4) | hex_digit(hex[1]);
  int g = (hex_digit(hex[2]) << 4) | hex_digit(hex[3]);
  int b = (hex_digit(hex[4]) << 4) | hex_digit(hex[5]);

  if (r < 0 || g < 0 || b < 0) {
    println("color: invalid hex");
    return;
  }

  char buf[32];
  buf[0] = '\033';
  buf[1] = '[';
  buf[2] = '3';
  buf[3] = '8';
  buf[4] = ';';
  buf[5] = '2';
  buf[6] = ';';
  int pos = 7;
  pos += itoa_s(r, buf + pos); buf[pos++] = ';';
  pos += itoa_s(g, buf + pos); buf[pos++] = ';';
  pos += itoa_s(b, buf + pos); buf[pos++] = 'm';
  buf[pos] = '\0';
  term_write(buf, (u32)pos);
}

/* ------------------------------------------------------------------ */
/*  built-in: kusr                                                    */
/* ------------------------------------------------------------------ */

static void cmd_kusr(void) {
  char pass[128];
  int pos = 0;

  print("kusr password: ");
  for (;;) {
    char c;
    long n = term_read(&c, 1);
    if (n <= 0) continue;
    if (c == '\r' || c == '\n') {
      printc('\n');
      pass[pos] = '\0';
      break;
    }
    if (c == '\b' || c == 0x7F) {
      if (pos > 0) pos--;
      continue;
    }
    if (c == 0x03) {
      print("^C\n");
      return;
    }
    if (c >= 32 && c < 127 && pos < 127)
      pass[pos++] = c;
  }

  long ret = kusr_auth(pass);
  for (int i = 0; i < pos; i++) pass[i] = 0;

  if (ret == 0) {
    g_kusr_authed = 1;
    println("kusr: authenticated");
  } else if (ret == -ERR_PERM) {
    println("kusr: wrong password");
  } else if (ret == -ERR_NOT_FOUND) {
    println("kusr: not configured (no kusr password set)");
  } else {
    print_err("kusr", ret);
  }
}

/* ------------------------------------------------------------------ */
/*  prompt                                                            */
/* ------------------------------------------------------------------ */

static void show_prompt(void) {
  char buf[MAX_PATH];
  long ret = fs_getcwd(buf, sizeof(buf));
  if (ret == 0) {
    print(buf);
  }
  print(" $ ");
}

/* ------------------------------------------------------------------ */
/*  command dispatch                                                  */
/* ------------------------------------------------------------------ */

static int exec_builtin(int argc, char **argv) {
  const char *cmd = argv[0];

  if (strcmp_s(cmd, "help") == 0)   { cmd_help(); return 0; }
  if (strcmp_s(cmd, "echo") == 0)   { cmd_echo(argc, argv); return 0; }
  if (strcmp_s(cmd, "pwd") == 0)    { cmd_pwd(); return 0; }
  if (strcmp_s(cmd, "cd") == 0)     { cmd_cd(argc, argv); return 0; }
  if (strcmp_s(cmd, "ls") == 0)     { cmd_ls(argc, argv); return 0; }
  if (strcmp_s(cmd, "ps") == 0)     { cmd_ps(); return 0; }
  if (strcmp_s(cmd, "cpus") == 0)   { cmd_cpus(); return 0; }
  if (strcmp_s(cmd, "time") == 0)   { cmd_time(); return 0; }
  if (strcmp_s(cmd, "mem") == 0)    { cmd_mem(); return 0; }
  if (strcmp_s(cmd, "cat") == 0)    { cmd_cat(argc, argv); return 0; }
  if (strcmp_s(cmd, "clear") == 0)  { cmd_clear(); return 0; }
  if (strcmp_s(cmd, "color") == 0)  { cmd_color(argc, argv); return 0; }
  if (strcmp_s(cmd, "kusr") == 0)   { cmd_kusr(); return 0; }
  if (strcmp_s(cmd, "drm_list") == 0) { cmd_drm_list(); return 0; }
  if (strcmp_s(cmd, "drm_switch") == 0) { cmd_drm_switch(argc, argv); return 0; }
  if (strcmp_s(cmd, "env") == 0)    { cmd_env(); return 0; }
  if (strcmp_s(cmd, "exit") == 0)   { return 1; }

  return -1;
}

static int exec_line(int argc, char **argv) {
  if (argc == 0) return 0;

  int r = exec_builtin(argc, argv);
  if (r >= 0) return r;

  char path[MAX_PATH];
  long rr = resolve_path(argv[0], path, sizeof(path));
  if (rr == 0) {
    run_external(path, argv, g_envp);
    return 0;
  }

  if (rr == -ERR_NOT_FOUND) {
    print("sh: command not found: ");
    println(argv[0]);
  } else {
    print("sh: ");
    print(argv[0]);
    print(": ");
    print(err_str(rr));
    print(" (code ");
    print_int((int)rr);
    print(")\n");
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/*  main loop                                                         */
/* ------------------------------------------------------------------ */

void _start(long argc, char **argv, char **envp) {
  (void)argc;
  (void)argv;
  syscall1(CALL_PERSONALITY, 0);
  g_envp = envp;

  println("otsos2 shell (sh) - type 'help' for commands");

  char line[MAX_LINE];
  char *pargv[MAX_ARGS];

  for (;;) {
    memset_s(line, 0, sizeof(line));
    for (int i = 0; i < MAX_ARGS; i++) pargv[i] = NULL;

    show_prompt();
    int len = read_line(line, sizeof(line));
    if (len < 0) {
      proc_exit(0);
    }

    int pargc = parse_line(line, pargv, MAX_ARGS);
    if (pargc == 0) continue;

    int rc = exec_line(pargc, pargv);
    if (rc == 1) {
      proc_exit(0);
    }
  }
}
