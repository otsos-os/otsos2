<!--
Copyright (c) 2026, otsos team

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->

# POSIX Syscalls (Kernel Layer)

This layer exposes a minimal POSIX-like API backed by ChainFS and kernel
devices. It manages a small `fd_table` of file descriptors.

## File Descriptors
- `0` = stdin, `1` = stdout, `2` = stderr (initialized at boot).
- Additional descriptors are allocated starting from `3`.
- Each descriptor tracks `path`, `offset`, and `flags`.

## Syscalls

### `open(path, flags) -> fd`
Opens a file by path and returns a new file descriptor.
- Supported flags: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`,
  `O_TRUNC`, `O_APPEND`.
- `O_CREAT` creates an empty file if it does not exist.
- `O_TRUNC` truncates the file to size 0 (requires write access).
- `O_APPEND` sets the file offset to the end before each write.

### `close(fd) -> 0/-1`
Releases the descriptor slot and clears its metadata.

### `read(fd, buf, count) -> bytes`
Reads starting at the current file offset and advances it.
- Returns `0` on EOF.
- `stdin` reads from the keyboard driver.

### `write(fd, buf, count) -> bytes`
Writes at the current file offset and advances it.
- `stdout`/`stderr` are sent to VGA and COM1.
- For file-backed descriptors, writes are merged into the existing file data.

