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
#define CALL_PROC_FORK   0x401
#define CALL_PROC_SPAWN  0x402
#define CALL_PROC_EXIT   0x403
#define CALL_PROC_WAIT   0x404
#define CALL_PROC_KILL   0x405

#define CALL_SYS_INFO    0x500
#define CALL_DRM_CALL    0x600
```

Returns are non-negative on success and negative `API_ERR_*` values on failure.

## Notes

- Terminal I/O is `termRead/termWrite`, not file descriptor `0/1/2`.
- ChainFS files and pipes use data handles.
- The kernel does not create terminal device nodes.
- `procSpawn` currently replaces the current process image; init uses it after
  `procFork`.
