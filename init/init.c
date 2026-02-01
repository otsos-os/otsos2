/*
 * init - First userspace process (PID 1)
 *
 * This is a minimal init for testing userspace functionality.
 * It uses syscalls to communicate with the kernel.
 */

/* Syscall numbers (must match kernel/syscall.h) */
#define SYS_WRITE 1
#define SYS_EXIT 60

/* File descriptors */
#define STDOUT 1

/* Inline syscall wrappers for x86_64 */
static long syscall1(long num, long arg1) {
  long ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(num), "D"(arg1)
                   : "rcx", "r11", "memory");
  return ret;
}

static long syscall3(long num, long arg1, long arg2, long arg3) {
  long ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
                   : "rcx", "r11", "memory");
  return ret;
}

/* Simple write wrapper */
static long write(int fd, const void *buf, unsigned long count) {
  return syscall3(SYS_WRITE, fd, (long)buf, count);
}

/* Simple exit wrapper */
static void exit(int code) {
  syscall1(SYS_EXIT, code);
  /* Should not return, but loop just in case */
  while (1) {
  }
}

/* Calculate string length */
static unsigned long strlen(const char *s) {
  unsigned long len = 0;
  while (s[len])
    len++;
  return len;
}

/* Print string to stdout */
static void print(const char *s) { write(STDOUT, s, strlen(s)); }

/* Entry point - must be at the start */
void _start(void) {
  print("================================\n");
  print("  OTSOS Init Process Started!\n");
  print("  Running in Ring 3 (userspace)\n");
  print("================================\n");
  print("\n");
  print("Hello from PID 1!\n");
  print("Init is working correctly.\n");
  print("\n");
  print("Exiting with code 0...\n");

  exit(0);
}
