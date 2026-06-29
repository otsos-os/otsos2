# Kernel ABI

## Overview

OTSOS userspace talks to the kernel through a small custom syscall table. The
CPU entry instruction is still x86-64 `syscall`, but the numbers and API are
not Linux/POSIX compatible.

## Registers

| Register | Purpose |
|----------|---------|
| RAX | call number in, return value out |
| RDI | arg 1 |
| RSI | arg 2 |
| RDX | arg 3 |
| RCX | user RIP saved by CPU |
| R11 | user RFLAGS saved by CPU |

## Calls

```c
#define CALL_TERM_READ   0x100
#define CALL_TERM_WRITE  0x101

#define CALL_DATA_OPEN   0x200
#define CALL_DATA_CLOSE  0x201
#define CALL_DATA_READ   0x202
#define CALL_DATA_WRITE  0x203
#define CALL_DATA_SEEK   0x204
#define CALL_DATA_PIPE   0x205

#define CALL_MEM_MAP     0x300

#define CALL_PROC_CLONE  0x400
#define CALL_PROC_COPY   0x401
#define CALL_PROC_SPAWN  0x402
#define CALL_PROC_EXIT   0x403
#define CALL_PROC_WAIT   0x404
#define CALL_PROC_KILL   0x405

#define CALL_PROC_THREAD_EXIT  0x40A
#define CALL_PROC_THREAD_JOIN  0x40B
#define CALL_PROC_GETTID       0x40C
#define CALL_PROC_EXIT_GROUP   0x40D
#define CALL_PROC_SET_TID_ADDR 0x40E
#define CALL_FUTEX_WAIT        0x40F
#define CALL_FUTEX_WAKE        0x410

#define CALL_SYS_INFO    0x500
#define CALL_DRM_CALL    0x600
```

Returns are non-negative on success and negative `API_ERR_*` values on failure.

## Notes

- Terminal I/O is `termRead/termWrite`, not file descriptor `0/1/2`.
- ChainFS files and pipes use data handles.
- The kernel does not create terminal device nodes.
- `procSpawn(path, argv, envp)` creates a new process and returns its PID.
- `procCopy` is a low-level process copy call for experiments; normal userspace
  should prefer `procSpawn`.
- `procClone(CLONE_VM|CLONE_THREAD, stack, 0)` creates a new thread in the
  current process. Returns the new TID to the parent, 0 to the child.
- `procThreadExit(code)` exits the calling thread. If it was the last thread,
  the whole process exits.
- `procThreadJoin(tid, &status)` waits for thread `tid` to terminate.
- `procGetTid()` returns the calling thread's TID.
- `procExitGroup(code)` kills the entire process (all threads) immediately.
- `procSetTidAddr(ptr)` sets the tid-clearing address. On thread exit the
  kernel writes 0 to `*ptr` and does a futex wake on it.
- `futexWait(uaddr, expected)` — if `*uaddr == expected`, sleep on `uaddr`.
- `futexWake(uaddr, max)` — wake up to `max` threads waiting on `uaddr`.
- `procExit(code)` is thread-aware: if >1 thread alive, exits just the
  calling thread; otherwise exits the whole process.
