# Kernel ABI Documentation

## Overview

This document describes the Application Binary Interface (ABI) for the OTSOS kernel. The ABI defines how user-space applications interact with the kernel through system calls, memory layout, and process management.

## Architecture

- **Target Architecture**: x86-64
- **Calling Convention**: System V AMD64 ABI for user-space
- **Kernel Mode**: Ring 0
- **User Mode**: Ring 3

## System Call Interface

### System Call Mechanism

The kernel uses the `syscall`/`sysret` instruction pair for fast system calls:

- **Entry Point**: Configured via MSR_LSTAR register
- **System Call Number**: Passed in `RAX` register
- **Arguments**: Passed in registers `RDI`, `RSI`, `RDX` (up to 3 arguments)
- **Return Value**: Returned in `RAX` register

### Register Usage

| Register | Purpose |
|----------|---------|
| RAX      | System call number (in), Return value (out) |
| RDI      | Argument 1 |
| RSI      | Argument 2 |
| RDX      | Argument 3 |
| RCX      | User RIP (saved by syscall) |
| R11      | User RFLAGS (saved by syscall) |
| RSP      | Switched to kernel stack during syscall |

### System Call Numbers

```c
#define SYS_READ  0   // read(fd, buf, count)
#define SYS_WRITE 1   // write(fd, buf, count)
#define SYS_OPEN  2   // open(pathname, flags)
#define SYS_CLOSE 3   // close(fd)
#define SYS_FORK  57  // fork()
#define SYS_EXECVE 59 // execve(path, argv, envp)
#define SYS_EXIT  60  // exit(status)
#define SYS_KILL  62  // kill(pid, sig)
```

### System Call Return Values

- **Success**: Non-negative value or 0
- **Error**: -1 (general error)

## Memory Layout

### Segment Selectors

| Segment | Selector | Ring | Description |
|---------|----------|------|-------------|
| Null    | 0x00     | -    | Null descriptor |
| Kernel Code | 0x08  | 0    | Kernel code segment (64-bit) |
| Kernel Data | 0x10  | 0    | Kernel data segment |
| User Data   | 0x18  | 3    | User data segment |
| User Code   | 0x20  | 3    | User code segment (64-bit) |
| TSS         | 0x28  | -    | Task State Segment |

### User Segment Selectors (with RPL)

```c
#define USER_CS 0x23  // 0x20 | 3
#define USER_DS 0x1B  // 0x18 | 3
```

### Process Memory

Each process has the following memory regions:

- **User Stack**: 64 KB (configurable via USER_STACK_SIZE)
- **Kernel Stack**: 16 KB per process (configurable via KERNEL_STACK_SIZE)
- **Entry Point**: Defined in ELF header or kernel function pointer

## Process Management

### Process Control Block (PCB)

```c
typedef struct process {
    u32 pid;                     // Process ID
    u32 ppid;                    // Parent Process ID
    process_state_t state;       // Current state
    char name[32];               // Process name
    
    // Memory
    u64 cr3;                     // Page table root
    u64 entry_point;             // Entry point address
    
    // Stack
    u64 kernel_stack;            // Kernel stack top
    u64 user_stack;              // User stack top
    
    // Context
    cpu_context_t context;       // Saved CPU state
    
    // Exit status
    int exit_code;
    
    struct process *next;        // For scheduler queue
} process_t;
```

### Process States

```c
typedef enum {
    PROC_STATE_UNUSED = 0,
    PROC_STATE_EMBRYO,     // Being created
    PROC_STATE_RUNNABLE,   // Ready to run
    PROC_STATE_RUNNING,    // Currently running
    PROC_STATE_SLEEPING,   // Waiting for event
    PROC_STATE_ZOMBIE      // Terminated, waiting for parent
} process_state_t;
```

### CPU Context

```c
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} cpu_context_t;
```

## Interrupt Handling

### Register Frame

During interrupts and system calls, the kernel saves a complete register frame:

```c
typedef struct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 int_no, err_code;
    u64 rip, cs, rflags, rsp, ss;
} registers_t;
```

## Task State Segment (TSS)

The TSS provides kernel stack switching for privilege level changes:

```c
typedef struct {
    u32 reserved0;
    u64 rsp0;        // Stack pointer for Ring 0
    u64 rsp1;        // Stack pointer for Ring 1 (unused)
    u64 rsp2;        // Stack pointer for Ring 2 (unused)
    u64 reserved1;
    u64 ist1;        // Interrupt Stack Table 1
    // ... ist2-ist7
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;  // I/O Map Base Address
} __attribute__((packed)) tss_t;
```

## System Call Implementation Details

### Entry Sequence

1. User executes `syscall` instruction
2. CPU saves `RCX` → `RIP`, `R11` → `RFLAGS`
3. CPU switches to kernel stack (from TSS.RSP0)
4. Kernel entry point saves all registers
5. Kernel calls `syscall_handler(regs)`

### Exit Sequence

1. Kernel restores all registers (except return value in `RAX`)
2. Kernel executes `sysret` instruction
3. CPU restores `RIP` from `RCX`, `RFLAGS` from `R11`
4. CPU switches back to user stack

### MSR Configuration

```c
#define MSR_EFER   0xC0000080  // Extended Feature Enable Register
#define MSR_STAR   0xC0000081  // System Call Target Address Register
#define MSR_LSTAR  0xC0000082  // Long Mode System Call Target Address
#define MSR_SFMASK 0xC0000084  // System Call Flag Mask
```

## Limits and Constraints

- **Maximum Processes**: 64 (configurable via MAX_PROCESSES)
- **Process Name Length**: 32 characters
- **User Stack Size**: 64 KB
- **Kernel Stack Size**: 16 KB per process

## POSIX Compatibility

The kernel provides a subset of POSIX-compatible system calls:

- File I/O: `read()`, `write()`, `open()`, `close()`
- Process management: `exit()`, `kill()`

## Notes for User-Space Developers

1. **Stack Alignment**: User stack should be 16-byte aligned before system calls
2. **Argument Limits**: Currently limited to 3 arguments per system call
3. **Error Handling**: Check return value for -1 to detect errors
4. **Signal Handling**: Basic signal support via `kill()` system call
5. **Memory Management**: Each process has separate virtual address space via CR3
