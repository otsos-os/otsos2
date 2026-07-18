# AGENTS

## Project

otsos2 is a hobbyist x86-64 operating system built from scratch. It has a custom
kernel, a bespoke userspace syscall ABI, a simple VFS, a custom ChainFS
filesystem, and enough POSIX-compatible code to run a musl-linked C program. The
repository is primarily an OS kernel + a few small userspace programs, with a
vendored musl libc under `libc/musl`.

The root of the repo also contains an unrelated `ai.py` (a tiny PyTorch
sentiment-classifier demo) and a separate `BoredOS` directory — both should be
treated as independent projects and not as part of otsos2.

## Tech Stack

- C (kernel, userspace, ports)
- Zig (ELF loader: `src/userland/elf.zig`, panic helper: `src/kernel/panic.zig`)
- GAS/NASM assembly (boot, interrupts, GDT, context switch, syscalls)
- Clang / LLVM toolchain (`clang`, `ld.lld`, `nasm`, `llvm-size`)
- Custom Multiboot2-compatible BIOS/UEFI loaders + `tools/makeiso.py` and
  `xorriso` for ISO creation
- QEMU for testing (UEFI via OVMF, optional virtio-gpu)
- TOML for kernel configuration (`src/config.toml`)
- musl libc (vendored, pre-configured for x86_64-linux-musl)

## Architecture

Monolithic kernel with the following rough layers:

1. Boot (`src/boot/boot.s`) — 32-bit protected mode, Multiboot2 entry point,
   identity/higher-half page tables, long mode switch, then `kmain()`.
2. Kernel core (`src/kernel/kernel.c`) — initializes memory, interrupts, timer,
   disk, filesystem, DRM, PCI, ACPI, userspace, and finally loads `init`.
3. Memory management (`src/kernel/mm/*`) — bootmem allocator, kernel heap
   (`kmem`), UMA-style allocator, VM object/page/map/pager, page tables (`pmap`).
4. Process/threading (`src/kernel/process.c`, `thread.c`, `scheduler.c`) —
   process table, threads, context switch, round-robin scheduler, signals, futex.
5. Drivers (`src/kernel/drivers/*`) — disk (ramdisk, optional PATA), ChainFS,
   VFS, devfs, keyboard, mouse, console, UART, timer, RTC, watchdog, ACPI,
   PCI, virtio-gpu, DRM/KMS.
6. Syscalls (`src/kernel/syscall.c`, `src/kernel/api/*`) — native ABI plus
   optional Linux/POSIX personality layer.
7. Userspace (`init/`, `ports/`) — small freestanding programs loaded as
   Multiboot modules and copied into `/bin` by the kernel.

## Main Subsystems

### Kernel (`src/kernel/`)

- `kernel.c` — entry point `kmain()`, boot sequence, module extraction, status
  lines, and hand-off to userspace.
- `api/` — native syscall implementations (`term_*`, `data_*`, `proc_*`,
  `fs_*`, `mem_*`, `sys_*`, `drm_*`, `event_*`, `kusr_*`, `posix/*`).
- `api/posix/` — Linux-compatible syscall personality when process
  `personality == PERSONALITY_POSIX`.  Syscall numbers track Linux x86-64
- `mm/` — `bootmem.c`, `kmem.c`, `uma.c`, `vm/pmap.c`, `vm/vm_page.c`,
  `vm/vm_object.c`, `vm/vm_map.c`, `vm/vm_pager.c`.
- `drivers/` — storage, filesystem, video, keyboard, timer, ACPI, power, PCI,
  watchdog. UART init uses a scratch-register probe; do not bring back the old
  loopback-only availability check.
- `drivers/fs/chainFS/` — custom filesystem used for root disk (512-byte blocks,
  superblock + file table + block map + chained data). Tooling in `chainfs.py`.
- `drivers/fs/vfs/` — tiny vnode-based VFS; used by POSIX personality and devfs.
- `drivers/fs/devfs/` — device nodes: `/dev/null`, `/dev/zero`, `/dev/random`,
  `/dev/urandom`, `/dev/tty`, `/dev/console`, `/dev/fb0`, `/dev/ptmx`,
  `/dev/pts/*`. `/dev/fb0` is a root-only Linux fbdev-compatible character
  device backed by the boot linear framebuffer.
- `drivers/video/drm/` — minimal DRM: GEM buffers, KMS objects, primary/cursor
  planes, atomic commit, fbdev, render helpers (`rapi`), and a virtio-gpu
  backend.
- `net/` — polling Ethernet/ARP/IPv4/ICMP stack; virtio-net RX is polled from
  timer IRQs after the network subsystem has initialized.
- `console/` — kernel terminal, pty, and console rendering.
- `event/` — kqueue-style event system (read, write, timer, proc, signal, user,
  keyboard and mouse filters).
- `kshell/` — optional kernel debug shell configured via `config.toml`.
- `crypto/` — SHA-256, HMAC-SHA256, PBKDF2, ChaCha20, RNG, plus `kusr`
  authentication.
- `interrupts/`, `gdt.c`, `time.c`, `futex.c`, `syscall.c`, `process.c`,
  `thread.c`, `scheduler.c`, `panic.c`, `useraddr.c`.

### Bootloader (`src/boot/`)

- `boot.s` — Multiboot2 header, graphical menu, PIT-based countdown, long mode
  setup, calls `kmain`. It is loaded by the custom BIOS/UEFI bootloaders.
- `boot/bootloader/bios/` — custom legacy BIOS loader in C/ASM, builds
  the BIOS El Torito image used by the hybrid ISO and `make run_bios`.
- `boot/bootloader/uefi/` — custom UEFI loader in Zig with a small ASM
  trampoline, builds the EFI image used by the hybrid ISO and `make run`.
- `boot/bootloader/lib/` — loader-agnostic helpers shared by BIOS and UEFI
  paths (bootpack, ELF64, Multiboot2, string).

### Minimal C library (`src/mlibc/`)

- Kernel-only C library: string, stdio, stdlib, hardware I/O, `kprintf`, `itoa`,
  and a small TOML parser used by `config.c`.
- Simple mlibc routines may be written in Kato and compiled through
  `etc/kato/src/main.py` into generated C objects during the kernel build.

### ELF loader (`src/userland/`)

- `elf.zig` — ELF parsing/loading in Zig.
- `userspace.c` — address-space setup, user stack allocation, and process
  creation.
- `userspace.asm` — assembly to enter Ring 3.

### Userspace programs (`init/`, `ports/`)

- `init/init.c` — first userspace process, event-driven program spawner via
  `procSpawn` and `procWait`.
- `ports/yes.c`, `fetch.c`, `shell.c`, `kbdtest.c`, `posix_hello.c`,
  `musl_test.c`, `demons/cursord.c` — small test/utility binaries and
  daemons.
- `ports/posix_hello.c` and `kbdtest.c` use Linux x86-64 syscall numbers and
  expect the POSIX personality.
- `ports/musl_test.c` uses real musl headers and is statically linked against
  vendored musl.
- `ports/lua/` — Lua interpreter port, uses musl's `Scrt1.o` and is dynamically
  linked as a PIE against shared `libc.so` (via `-pie -dynamic-linker /lib/ld-musl-x86_64.so.1`).
  Patched via `ports/lua/diff.patch`, set up by `ports/lua/setup.sh`.

### Vendored libc (`libc/musl/`)

- Configured for x86_64-linux-musl with shared libraries enabled.
- Builds both `lib/libc.a` (static) and `lib/libc.so` (shared + dynamic linker).
- The dynamic linker (`ld-musl-x86_64.so.1`) is `libc.so` itself (musl unified model).
- `lib/Scrt1.o` — PIE startup crt, used by dynamically linked programs.
- The kernel build optionally links `ports/musl_test` statically against it.
  Do not modify upstream musl unless you know what you are doing.

### Native libc (`libc/native/`)

- Native C runtime for the native userspace ABI, separate from musl and separate from
  POSIX compatibility.
- Public API uses kernel-native names: `term*`, `data*`, `fs*`, `proc*`,
  `event*`, `sys*`, `drm*`.
- Source files are split by subsystem (`data.c`, `fs.c`, `proc.c`,
  `event.c`, `sys.c`, `drm.c`, etc.); do not collapse them into one wrapper
  file.
- `stdio` is buffered
- Builds `lib/crt0.o` and `lib/libc.a` for freestanding userspace binaries.
- Keep POSIX-only work on musl / personality layer; do not blur the two.

### Native archive library (`libc/LibArchive`)

- Small native userspace archive library built on top of `libc/native`.
- Currently exports `la_zip_extract()` for ZIP extraction.  Supported ZIP
  method is stored/no-compression only;
### Dynamic linking support

- The kernel already handles `PT_INTERP` in both `spawn.c` (`api_proc_spawn`)
  and `posix_proc.c` (`posix_execve`):
  - Loads the main ELF via `elf_load_full`.
  - If `li.interp_off != 0`, reads the interpreter path from the ELF.
  - Loads the interpreter binary from the filesystem at `ELF_INTERP_BASE` (0x40000000).
  - Sets `aux.at_base = ELF_INTERP_BASE` and entry point to interpreter entry.
  - The dynamic linker (musl's `libc.so`) performs symbol resolution and jumps
    to the program's `_start`.
- The dynamic linker is installed as a multiboot module `ld-musl` →
  `/lib/ld-musl-x86_64.so.1` (copied from `libc.so`).
- Programs using dynamic linking must be built as PIE (`-pie`), linked against
  musl's `Scrt1.o` and `-lc` (shared), with `-dynamic-linker /lib/ld-musl-x86_64.so.1`.
- `libc/LibExec/dld` contains experimental native `drld`, a small C runtime
  loader built against `libc/native` and installed as `drld` →
  `/lib/drld`.  It is intended for simple ELF64 `.so` linking via native
  syscalls, not as a full musl `ld.so` replacement yet.

### Configuration

- `src/config.toml` — kernel identity, timer frequency, kshell, libc toggle, disk
  options, and multiboot module list. `config.c` reads it at boot from the
  `config` Multiboot module and can persist it to `/conf/boot/modules.toml`.
- `[modules]` section in `config.toml` — maps multiboot module names to
  filesystem destinations (e.g., `yes = "/bin/yes"`).  The kernel iterates this
  section at boot to copy modules into ChainFS; the module named `init` is also
  used to start the first userspace process.  The Makefile uses the same list
  via `tools/modules.pl` when building the bootpack for the custom loaders.
- The `ld-musl` module in `[modules]` provides the musl dynamic linker at
  `/lib/ld-musl-x86_64.so.1` for dynamically linked programs.

### Tooling / scripts

- `chainfs.py` — inspect/format/mount ChainFS images from the host.
- `add_copyright.sh` — prepends the BSD-2-Clause copyright header to `.c`,
  `.h`, `.s` files.
- `tools/toml_get.sh` — helper used by Makefiles to read `config.toml` values.
- `tools/modules.pl` — extracts module names from the `[modules]` section of
  `config.toml`; used by the Makefile to build the bootpack consumed by both
  custom bootloaders.
- `tools/makeiso.py` — small `xorriso` wrapper that creates the hybrid
  BIOS+UEFI ISO from the BIOS disk image and EFI system partition image.

## Repository Structure

- `src/` — kernel, bootloader, mlibc, userland ELF loader.
- `init/` — first userspace process (`init`) and `hello` test.
- `ports/` — additional userspace programs.
- `libc/musl/` — vendored musl libc source + prebuilt objects.
- `bin/` — build artifacts (objects, `kernel.bin`, `disk.img`, `.map`).
- `tools/` — helper scripts for the build.
- `chainfs.py` — host-side ChainFS tool.
- `add_copyright.sh` — license header script.
- `ddps.md` — project-specific C coding style (DDPS / FreeBSD-like).
- `readme.md` — short human-oriented description.
- `src/kernel/abi.md` — native syscall register ABI.
- `src/mlibc/spec.md` — mlibc overview.

## Important Entry Points

- `src/boot/boot.s:457` — `start` label, earliest boot code.
- `src/kernel/kernel.c:310` — `kmain()` — kernel main.
- `src/kernel/syscall.c:60` — `syscall_init()` — syscall/sysret setup.
- `src/kernel/syscall.c:86` — `syscall_handler()` — dispatch.
- `src/kernel/api/posix/posix.c:160` — `posix_syscall_handler()` — POSIX
  personality dispatch.
- `src/userland/userspace.c:56` — `userspace_init()` — enable Ring 3.
- `src/userland/userspace.c:139` — `userspace_load_elf()` — create a process.
- `init/init.c:250` — `_start()` — first userspace process.
- `src/kernel/scheduler.c` — `scheduler_tick()` — timer-driven preemption.
- `src/kernel/drivers/fs/chainFS/chainfs.c:91` — `chainfs_init()` — root FS.
- `src/kernel/drivers/video/drm/drm.c` — DRM core.
- `src/kernel/api/api.h` — native syscall structs and constants.
- `src/kernel/api/posix/posix.h` — POSIX syscall numbers and structs.
- `src/kernel/syscall.h` — native syscall numbers (`CALL_*`).

## Development Workflow

Build from `src/`:

```bash
cd src
make          # interactive version prompt; builds kernel + init + ports + ISO
make run      # QEMU with UEFI + GUI, using the custom loader
make run_nodis # QEMU with UEFI, no display
make run_bios # QEMU BIOS mode, same hybrid ISO
make virtio   # QEMU with virtio-gpu
make debug    # QEMU with `-d int,cpu_reset`
make clean
```

The Makefile expects: `clang`, `ld.lld`, `nasm`, `zig`, `xorriso`, `mtools`,
`python3`, `llvm-size`, `qemu-system-x86_64`, and OVMF at
`/usr/share/ovmf/OVMF.fd`.

User need to test, dont run test manually, ask user. 

## Conventions

### Style (DDPS)

- Read `ddps.md` before writing any C code.
- Every `.c`/`.h` file must start with `/* !DEFINES! ... */` and
  `/* !SPACE! ... */` manifest blocks describing types/functions and their
  visibility (`%internal` / `%export`).
- BSD-2-Clause copyright header is required; use `add_copyright.sh` if missing.
- Hard tabs for indentation, 80-column limit, K&R braces (`if (...) {`),
  function opening brace on its own line.
- Function declaration style: return type on its own line, then function name and
  args. Example: `void\nfoo(int x)`.
- Local variables declared at the top of the block, sorted by size (largest
  first), grouped by semantic purpose. No mid-block declarations.
- No space between `*` and pointer variable name: `char *str;`.
- Use `return (value);` form.
- Avoid complex initializers in declaration blocks.

### Naming and file organization

- Headers: `<kernel/...>` for kernel headers, `<mm/...>` for memory headers,
  `<mlibc/...>` for mlibc, `<userland/...>` for userland.
- Include paths are set via `-I. -Ikernel -Ikernel/drivers/video` in the main
  Makefile.
- Kernel modules are usually one `.c` + `.h` pair per logical unit; larger
  subsystems (DRM, POSIX) get subdirectories.
- Public API is declared in headers; implementation files are named after the
  subsystem (e.g., `kernel/api/write.c` implements `api_term_write`/`api_data_write`
  depending on the call).

### Critical invariants

- Do not touch `kernel.c` boot order without understanding the memory-map and
  Multiboot module layout.
- ChainFS is a flat, single-chain filesystem; it is not POSIX-safe for hard
  links or large files. POSIX personality uses the VFS and devfs on top of it.
- POSIX personality is opt-in per process via `CALL_PERSONALITY` (`0xFFFF`);
  default is native `PERSONALITY_OTSOS`.
- POSIX `mremap(2)` is implemented in `api/posix/posix_mem.c` on top of
  VMA clipping/relocation helpers in `mm/vm/vm_map.c`; `MREMAP_DONTUNMAP`
  deliberately returns `EINVAL` because its Linux userfaultfd semantics are not
  modeled yet.
- `src/config.toml` is parsed into a global TOML document; many compile-time
  feature flags (`kshell`, `libc`, `pata`) are also read from it by the Makefile
  via `tools/toml_get.sh`.
- **Privilege model:** native `kusr_auth` and POSIX `euid == 0` are treated as
  the same root privilege inside the kernel (`proc_has_privilege()`).  Init and
  kernel processes start as root; children inherit credentials on fork/clone/
  copy/spawn.  Non-root / non-kusr processes are blocked from `/conf` and from
  dangerous syscalls (kmeminfo, DRM master ops, terminal disable/reset).  POSIX
  programs can drop or swap credentials via `setuid`/`setgid` (syscalls 105/106).
- `/dev/fb0` follows the same root privilege check in its vnode operations and
  native open path; POSIX mode is `0600`, and POSIX `mmap()` maps the device via
  a device-backed VM object.

## Dependencies Between Modules

- `boot.s` → `kernel.c` (`kmain`)
- `kernel.c` → memory (`mm/*`), interrupts (`idt`), timer, disk, ChainFS, VFS,
  DRM, PCI, ACPI, power, userspace, kshell, config, crypto, syscalls.
- `syscall.c` → `api/*` handlers; for `PERSONALITY_POSIX` it delegates to
  `posix_syscall_handler()`.
- `process.c`/`thread.c`/`scheduler.c` → each other, `mm/vm/pmap.h`, `idt.h`.
- `userspace.c` → `elf.zig`, `pmap`, `vm_page`, `vm_map`, `vm_object`,
  `process`, `thread`, `gdt`.
- `api` layer → VFS/devfs/ChainFS, terminal/DRM, process/thread, memory, time.
- `drivers/fs/vfs` → `drivers/fs/devfs` and `drivers/fs/chainFS`.
- `drivers/video/drm` → `drivers/video/card/virtio-gpu`.
- `kshell` → `console`, `terminal`, `keyboard`, `drm`, `config`.
- `crypto` → `drivers/timer` for RNG entropy; `kusr` → `crypto` for
  PBKDF2/HMAC auth.


## Things Every Agent Should Know

- otsos2 is a single-address-space kernel during boot; userspace processes get
  their own page tables via `pmap_create()`.
- The kernel is loaded at `0xFFFFFFFF80000000` (higher half) after the bootloader
  sets up identity + higher-half mappings. See `src/linker.ld`.
- Userspace programs are loaded at `0x400000` and use a stack at
  `0x00007FFFFFFFFFF0`. See `init/linker.ld`.
- The kernel.c copies Multiboot modules (`init`, `yes`, `sh`, `fetch`,
  `posix_hello`, `kbdtest`, optional `musl_test`) into `/bin/` on the root
  filesystem before starting `init`. These are not on disk; they come from the
  ISO.
- The `config.toml` is both a build-time config (read by Makefiles) and a
  runtime config (read by the kernel `src/kernel/other/config.c`). Changing it can affect which modules are
  built or which features are enabled.
- `libc/musl` is vendored and pre-built. Do not run its `configure` again unless
  you want to rebuild `config.mak` from scratch; instead edit `config.mak` if
  needed.
## Notes

- The native syscall ABI is documented in `src/kernel/abi.md`: RAX = number,
  RDI/RSI/RDX = first three args, syscall/sysret, returns in RAX. POSIX
  personality uses Linux x86-64 calling convention (R10/R8/R9 for args 4–6).
- ChainFS superblock magic is `0xCAFEBABE`, block size is 512 bytes.
- There is no CI, no automated test harness, and no package manager. The build
  is purely Makefile-based.


If someone changes in otsos please update this file.
