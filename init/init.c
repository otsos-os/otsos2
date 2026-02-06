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

/*
 * init - First userspace process (PID 1)
 */

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_EXIT 60
#define STDIN 0
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

static long read(int fd, void *buf, unsigned long count) {
  return syscall3(SYS_READ, fd, (long)buf, count);
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
  print("\n");
  print("Hello init\n");

  print("Test: read into kernel memory (should be killed)...\n");
  read(STDIN, (void *)0xFFFFFFFF80000000ULL, 4);

  while (1) {
    char buffer[128];
    print("vedi chototo: ");
    long bytes = read(STDIN, buffer, 100);

    if (bytes > 0) {
      buffer[bytes] = 0;
      print("ti vvel: ");
      print(buffer);
      print("\n");
    }
  }
}
