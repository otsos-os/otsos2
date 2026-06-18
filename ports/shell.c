/*
 * sh - otsos2 userspace shell
 *
 * Built-in commands: echo, pwd, cd, ls, ps, cat, clear, exit, help, env
 * External commands: spawn + wait via CALL_PROC_SPAWN/CALL_PROC_WAIT
 * PATH resolution: /bin/<cmd> for external binaries
 *
 * Freestanding: no libc, raw syscalls via inline asm.
 * (deepseek code)
 */

#define CALL_TERM_READ  0x100
#define CALL_TERM_WRITE 0x101
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
#define CALL_SYS_INFO   0x500

#define API_OPEN_READ   0x0001

#define MAX_LINE  512
#define MAX_ARGS  64
#define MAX_PATH  256
#define MAX_DIRENT 128
#define MAX_PROCS 64

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

static char **g_envp;
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
  return syscall3(CALL_TERM_READ, (long)buf, (long)len, 0);
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

static int file_exists(const char *path) {
  int fd = data_open(path, API_OPEN_READ);
  if (fd < 0) return 0;
  data_close(fd);
  return 1;
}

static int resolve_path(const char *cmd, char *out, u32 out_sz) {
  if (cmd[0] == '/' || cmd[0] == '.') {
    u32 len = strlen_s(cmd);
    if (len >= out_sz) return -1;
    memcpy_s(out, cmd, len + 1);
    return file_exists(out) ? 0 : -1;
  }

  const char *prefix = "/bin/";
  u32 plen = strlen_s(prefix);
  u32 clen = strlen_s(cmd);
  if (plen + clen >= out_sz) return -1;
  memcpy_s(out, prefix, plen);
  memcpy_s(out + plen, cmd, clen + 1);
  return file_exists(out) ? 0 : -1;
}

static int run_external(const char *path, char **argv, char **envp) {
  long pid = proc_spawn(path, (const char *const *)argv,
                        (const char *const *)envp);
  if (pid < 0) {
    print("sh: failed to spawn ");
    println(path);
    return -1;
  }

  int status;
  for (;;) {
    long w = proc_wait(&status);
    if (w > 0) break;
    if (w < 0 && w != -10) {
      print("sh: wait error: ");
      print_int((int)w);
      printc('\n');
      return -1;
    }
  }
  return 0;
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
  println("  cat <file>         print file contents");
  println("  clear              clear screen");
  println("  color <hex>        set text color (e.g. FF0000)");
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
    println("pwd: error");
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
    println(path);
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
    print("ls: error ");
    print_int((int)n);
    printc('\n');
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
/*  built-in: ps                                                      */
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
    println("ps: error");
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
    println(argv[1]);
    return;
  }
  char buf[256];
  for (;;) {
    long n = data_read(fd, buf, sizeof(buf));
    if (n < 0) break;
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
  if (strcmp_s(cmd, "cat") == 0)    { cmd_cat(argc, argv); return 0; }
  if (strcmp_s(cmd, "clear") == 0)  { cmd_clear(); return 0; }
  if (strcmp_s(cmd, "color") == 0)  { cmd_color(argc, argv); return 0; }
  if (strcmp_s(cmd, "env") == 0)    { cmd_env(); return 0; }
  if (strcmp_s(cmd, "exit") == 0)   { return 1; }

  return -1;
}

static int exec_line(int argc, char **argv) {
  if (argc == 0) return 0;

  int r = exec_builtin(argc, argv);
  if (r >= 0) return r;

  char path[MAX_PATH];
  if (resolve_path(argv[0], path, sizeof(path)) == 0) {
    run_external(path, argv, g_envp);
    return 0;
  }

  print("sh: command not found: ");
  println(argv[0]);
  return 0;
}

/* ------------------------------------------------------------------ */
/*  main loop                                                         */
/* ------------------------------------------------------------------ */

void _start(long argc, char **argv, char **envp) {
  (void)argc;
  (void)argv;
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
