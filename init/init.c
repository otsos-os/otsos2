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
 * init — First userspace process (PID 1)
 *
 * Event-driven using kqueue/kevent. Watches stdin (EVFILT_READ) for
 * user commands and child processes (EVFILT_PROC) for exit notifications.
 * No busy-loops — the kernel wakes init when events occur.
 */

#define CALL_TERM_READ  0x100
#define CALL_TERM_WRITE 0x101
#define CALL_PROC_SPAWN 0x402
#define CALL_PROC_WAIT  0x404
#define CALL_PROC_EXIT  0x403
#define CALL_EVENT_KQUEUE 0x700
#define CALL_EVENT_KEVENT 0x701
#define CALL_EVENT_CLOSE  0x702

/* Event system constants — must match kernel/event/event.h */
#define EVFILT_READ   (-1)
#define EVFILT_TIMER  (-3)
#define EVFILT_PROC   (-4)

#define EV_ADD     0x0001
#define EV_DELETE  0x0002
#define EV_ENABLE  0x0004
#define EV_ONESHOT 0x0010
#define EV_CLEAR   0x0020
#define EV_EOF     0x8000

#define NOTE_EXIT  0x80000000U

struct kevent {
  unsigned long long ident;
  short filter;
  unsigned short flags;
  unsigned int fflags;
  long long data;
  unsigned long long udata;
};

struct kevent_args {
  int kq_idx;
  struct kevent *changelist;
  int nchanges;
  struct kevent *eventlist;
  int nevents;
  long long timeout_ms;
};

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

static long procSpawn(const char *path, char *const argv[],
                      char *const envp[]) {
  return syscall3(CALL_PROC_SPAWN, (long)path, (long)argv, (long)envp);
}

static long procWait(int *status) {
  return syscall1(CALL_PROC_WAIT, (long)status);
}

static int kqueue_create(void) {
  return (int)syscall1(CALL_EVENT_KQUEUE, 0);
}

static int kqueue_close(int kq) {
  return (int)syscall1(CALL_EVENT_CLOSE, (long)kq);
}

static int kevent(int kq, struct kevent *changes, int nchanges,
                  struct kevent *events, int nevents,
                  long long timeout_ms) {
  struct kevent_args args;
  args.kq_idx = kq;
  args.changelist = changes;
  args.nchanges = nchanges;
  args.eventlist = events;
  args.nevents = nevents;
  args.timeout_ms = timeout_ms;
  return (int)syscall3(CALL_EVENT_KEVENT, 0, (long)&args, 0);
}

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

static int strcmp(const char *a, const char *b) {
  while (*a && *b && *a == *b) {
    a++;
    b++;
  }
  return *a - *b;
}

void _start(void) {
  print("\n");
  print("Hello init (event-driven)\n");

  int kq = kqueue_create();
  if (kq < 0) {
    print("init: kqueue_create failed, falling back to polling\n");
    kq = -1;
  }

  /* Watch stdin (fd 0) for readable events */
  struct kevent changes[2];
  struct kevent events[8];

  if (kq >= 0) {
    changes[0].ident = 0;           /* fd 0 = stdin */
    changes[0].filter = EVFILT_READ;
    changes[0].flags = EV_ADD | EV_CLEAR;
    changes[0].fflags = 0;
    changes[0].data = 0;
    changes[0].udata = 0;

    kevent(kq, changes, 1, 0, 0, -1);
  }

  int child_pid = -1;

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
    child_pid = (int)pid;

    /* Register EVFILT_PROC to watch for child exit */
    if (kq >= 0) {
      changes[0].ident = (unsigned long long)child_pid;
      changes[0].filter = EVFILT_PROC;
      changes[0].flags = EV_ADD | EV_ONESHOT;
      changes[0].fflags = NOTE_EXIT;
      changes[0].data = 0;
      changes[0].udata = (unsigned long long)child_pid;

      kevent(kq, changes, 1, 0, 0, -1);

      /* Block until child exits — no busy loop! */
      int n = kevent(kq, 0, 0, events, 8, -1);
      if (n > 0) {
        for (int i = 0; i < n; i++) {
          if (events[i].filter == EVFILT_PROC &&
              events[i].fflags & NOTE_EXIT) {
            int exit_code = (int)events[i].data;
            print("child exited\n");
            (void)exit_code;
          }
        }
      }
    } else {
      /* Fallback: busy-wait on procWait */
      int status = 0;
      while (procWait(&status) < 0) {
      }
    }

    /* Reap the zombie */
    int status = 0;
    procWait(&status);
    child_pid = -1;
  }
}
