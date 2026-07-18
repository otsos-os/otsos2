# Kernel ABI

## Overview

OTSOS userspace talks to the kernel through a small custom syscall table. The
CPU entry instruction is still x86-64 `syscall`

## Registers

| Register | Purpose |
|----------|---------|
| RAX | call number in, return value out |
| RDI | arg 1 |
| RSI | arg 2 |
| RDX | arg 3 |
| R10 | arg 4 |
| R8 | arg 5 |
| R9 | arg 6 |
| RCX | user RIP saved by CPU |
| R11 | user RFLAGS saved by CPU |
