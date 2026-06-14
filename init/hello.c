/*
 * hello - simple userspace test program
 */

#define CALL_TERM_WRITE 0x101
#define CALL_PROC_EXIT 0x403

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

static long termWrite(const void *buf, unsigned long count) {
  return syscall3(CALL_TERM_WRITE, (long)buf, count, 0);
}

static void procExit(int code) {
  syscall1(CALL_PROC_EXIT, code);
  while (1) {
  }
}

static unsigned long strlen(const char *s) {
  unsigned long len = 0;
  while (s[len])
    len++;
  return len;
}

void _start(void) {
  const char *msg = "Hello from hello.bin\n";
  termWrite(msg, strlen(msg));
  procExit(0);
}
