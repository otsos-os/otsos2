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

#define CALL_TERM_READ 0x100
#define CALL_TERM_WRITE 0x101
#define CALL_PROC_SPAWN 0x402
#define CALL_PROC_WAIT 0x404

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

static long termRead(void *buf, unsigned long count) {
  return syscall3(CALL_TERM_READ, (long)buf, count, 0);
}

static long procSpawn(const char *path, char *const argv[], char *const envp[]) {
  return syscall3(CALL_PROC_SPAWN, (long)path, (long)argv, (long)envp);
}

static long procWait(int *status) { return syscall1(CALL_PROC_WAIT, (long)status); }

static unsigned long strlen(const char *s) {
  unsigned long len = 0;
  while (s[len])
    len++;
  return len;
}

static void print(const char *s) { termWrite(s, strlen(s)); }

static void trim_newline(char *s) {
  unsigned long i = 0;
  while (s[i]) {
    if (s[i] == '\n' || s[i] == '\r') {
      s[i] = 0;
      return;
    }
    i++;
  }
}

void _start(void) {
  print("\n");
  print("Hello init\n");

  while (1) {
    char path[128];
    print("Enter program path (relative, e.g. hello): ");
    long bytes = termRead(path, 120);
    if (bytes <= 0) {
      continue;
    }
    if (bytes >= 120) {
      bytes = 119;
    }
    path[bytes] = 0;
    trim_newline(path);

    if (path[0] == 0) {
      print("empty path\n");
      continue;
    }

    char *argv[2];
    argv[0] = path;
    argv[1] = 0;

    long pid = procSpawn(path, argv, 0);
    if (pid < 0) {
      print("procSpawn failed\n");
      continue;
    }

    print("child running\n");
    int status = 0;
    while (procWait(&status) < 0) {
    }
  }
}
