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
#define SYS_FORK 57
#define SYS_EXECVE 59
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

static long syscall0(long num) {
  long ret;
  __asm__ volatile("syscall" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
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

static long fork(void) { return syscall0(SYS_FORK); }

static long execve(const char *path, char *const argv[], char *const envp[]) {
  return syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
}

static unsigned long strlen(const char *s) {
  unsigned long len = 0;
  while (s[len])
    len++;
  return len;
}

static void print(const char *s) { write(STDOUT, s, strlen(s)); }

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
    long bytes = read(STDIN, path, 120);
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

    long pid = fork();
    if (pid == 0) {
      execve(path, argv, 0);
      print("execve failed (child)\n");
      exit(1);
    }

    if (pid < 0) {
      print("fork failed\n");
      continue;
    }

    print("fork ok, child running\n");
  }
}
