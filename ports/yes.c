/*
 * yes - output a string repeatedly until killed (Otsos userspace)
 *
 * Minimal freestanding port for Otsos (no libc).
 */

#define SYS_WRITE 1
#define SYS_EXIT 60
#define STDOUT 1

typedef unsigned long usize;

static long syscall1(long num, long arg1) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(num), "D"(arg1)
                   : "rcx", "r11", "memory");
  return ret;
}

static long syscall3(long num, long arg1, long arg2, long arg3) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
                   : "rcx", "r11", "memory");
  return ret;
}

static long write_sys(int fd, const void *buf, usize count) {
  return syscall3(SYS_WRITE, fd, (long)buf, count);
}

static void exit_sys(int code) {
  syscall1(SYS_EXIT, code);
  while (1) {
  }
}

static usize strlen_sys(const char *s) {
  usize len = 0;
  while (s[len])
    len++;
  return len;
}

static int write_all(int fd, const char *buf, usize len) {
  while (len) {
    long n = write_sys(fd, buf, len);
    if (n <= 0)
      return -1;
    buf += (usize)n;
    len -= (usize)n;
  }
  return 0;
}

static void yes_write_pieces(int argc, char **argv) {
  for (;;) {
    for (int i = 1; i < argc; i++) {
      if (write_all(STDOUT, argv[i], strlen_sys(argv[i])) != 0)
        exit_sys(1);
      if (i + 1 < argc) {
        if (write_all(STDOUT, " ", 1) != 0)
          exit_sys(1);
      }
    }
    if (write_all(STDOUT, "\n", 1) != 0)
      exit_sys(1);
  }
}

static void yes_write_buffered(int argc, char **argv) {
  /* Build a single line once and write it repeatedly. */
  char buf[4096];
  usize used = 0;
  for (int i = 1; i < argc; i++) {
    const char *s = argv[i];
    usize len = strlen_sys(s);
    for (usize j = 0; j < len; j++)
      buf[used++] = s[j];
    if (i + 1 < argc)
      buf[used++] = ' ';
  }
  buf[used++] = '\n';

  for (;;) {
    if (write_all(STDOUT, buf, used) != 0)
      exit_sys(1);
  }
}

void _start(long argc, char **argv, char **envp) {
  (void)envp;

  if (argc <= 1 || !argv) {
    const char *msg = "y\n";
    for (;;) {
      volatile int a = 2;
      volatile int b = 0;
      int ressss = a / b;
      if (write_all(STDOUT, msg, 2) != 0)
        exit_sys(1);
    }
  }

  /* Decide if arguments fit in a small buffer. */
  usize need = 1; /* newline */
  for (int i = 1; i < argc; i++) {
    need += strlen_sys(argv[i]);
    if (i + 1 < argc)
      need += 1; /* space */
  }
  

  if (need <= 4096)
    yes_write_buffered(argc, argv);
  else
    yes_write_pieces(argc, argv);
}
