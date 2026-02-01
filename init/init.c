/*
 * init - First userspace process (PID 1)
 */

#define SYS_WRITE 1
#define SYS_EXIT 60
#define STDOUT 1

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

static long write(int fd, const void *buf, unsigned long count) {
  return syscall3(SYS_WRITE, fd, (long)buf, count);
}

static void exit(int code) {
  syscall1(SYS_EXIT, code);
  while (1) {
  }
}

static unsigned long strlen(const char *s) {
  unsigned long len = 0;
  while (s[len])
    len++;
  return len;
}

static void print(const char *s) { write(STDOUT, s, strlen(s)); }

void _start(void) {
  print("================================\n");
  print("  OTSOS Init Process (SYSCALL)\n");
  print("  Running in Ring 3 (native x64)\n");
  print("================================\n");
  print("\n");
  print("Hello from PID 1 via SYSCALL instruction!\n");

  while (1) {
    __asm__ volatile("hlt");
  }
}
