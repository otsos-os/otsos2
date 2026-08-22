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

- C (kernel, userspace, native programs, ports)
- Zig (ELF loader: `src/userland/elf.zig`, panic helper: `src/kernel/panic.zig`)
- GAS/NASM assembly (boot, interrupts, GDT, context switch, syscalls)
- Clang / LLVM toolchain (`clang`, `ld.lld`, `nasm`, `llvm-size`)
- Custom Multiboot2-compatible BIOS/UEFI loaders + `tools/makeiso.py` and
  `xorriso` for ISO creation
- QEMU for testing (UEFI via OVMF, optional virtio-gpu)
- TOML for build-time Makefile configuration (`src/config.toml`)
- Registry hive DSL under `config/hives/`, compiled by `tools/hivec.py` into
  the `cmseed` boot module consumed by the runtime configuration manager
- musl libc (vendored, pre-configured for x86_64-linux-musl)

## Architecture

Monolithic kernel with the following rough layers:

1. Boot (`src/boot/boot.s`) — 32-bit protected mode, Multiboot2 entry point,
   identity/higher-half page tables, long mode switch, then `kmain()`.
2. Kernel core (`src/kernel/kernel.c`) — initializes core services, boot memory,
   interrupts, and newbus, then lets registered drivers attach by pass before
   loading `init`.
3. Memory management (`src/kernel/mm/*`) — bootmem allocator, kernel heap
   (`kmem`), UMA-style allocator, VM object/page/map/pager, page tables (`pmap`).
4. Process/threading (`src/kernel/process.c`, `thread.c`, `scheduler.c`) —
   process table, threads, context switch, round-robin scheduler, signals, futex.
5. Drivers (`src/kernel/drivers/*`) — newbus-managed storage, filesystems,
   devfs nodes, input, console, UART, timers, RTC, watchdog, PMU, ACPI, PCI,
   network, DRM/KMS, and display backends.
6. Syscalls (`src/kernel/syscall.c`, `src/kernel/api/*`) — native ABI plus
   optional Linux/POSIX personality layer.
7. Entity manager (`src/kernel/entity/`) — data-oriented entity store with
   parallel SoA columns, per-process handle tables, and a native-only
   `/Entity` namespace.
8. Userspace (`init/`, `progs/`, `ports/`) — small freestanding programs loaded as
   Multiboot modules and copied into `/bin` by the kernel.

## Main Subsystems

### Kernel (`src/kernel/`)

- `kernel.c` — entry point `kmain()`, boot sequence, module extraction, status
  lines, and hand-off to userspace.
- `api/` — native syscall implementations (`term_*`, `data_*`, `proc_*`,
  `fs_*`, `mem_*`, `sys_*`, `drm_*`, `event_*`, `kusr_*`, `posix/*`).
- `api/posix/` — Linux-compatible syscall personality when process
  `personality == PERSONALITY_POSIX`.  Syscall numbers track Linux x86-64.
  POSIX sockets support AF_UNIX and AF_INET TCP/UDP; AF_INET routes through the
  native `net_endpoint_*` stack rather than a separate socket stack.
- `mm/` — `bootmem.c`, `kmem.c`, `uma.c`, `vm/pmap.c`, `vm/vm_page.c`,
  `vm/vm_object.c`, `vm/vm_map.c`, `vm/vm_pager.c`.
- `drivers/` — storage, filesystem, video, keyboard, timer, ACPI, power, PCI,
  watchdog, PMU. Early serial console setup goes through `console_early_init()`;
  UART's full driver attach is a newbus ISA module. UART probing uses a
  scratch-register probe; do not bring back the old loopback-only availability
  check.
- `drivers/newbus/` — FreeBSD-like driver model. Concrete drivers self-register
  with `DRIVER_MODULE`, `PCI_DRIVER_MODULE`, `ISA_DRIVER_MODULE`,
  `FIRMWARE_DRIVER_MODULE`, `PLATFORM_DRIVER_MODULE`, or
  `PSEUDO_DRIVER_MODULE`; attach ordering is controlled by pass/order. Keep
  `kernel.c`, `interrupts/handlers.c`, `drivers/newbus/nexus.c`, PCI core, DRM
  core, VFS core, and DevFS registries free of concrete driver enumeration.
  Drivers may also expose named I/O interfaces via
  `newbus_interface_register`/`newbus_interface_unregister`
  (`src/kernel/drivers/newbus/interface.c`); an interface is a per-device
  table of read/write/ioctl/stat callbacks. Each registered interface and
  each visible device gets a persistent entity: devices live at
  `/Entity/Interface/Driver/<devunit>` and interfaces at
  `/Entity/Interface/Driver/<devunit>/<iface>`. The namespace is
  native-ABI-only, is not mounted in VFS, and is invisible to the POSIX
  personality, which keeps using `/dev`. Entity handles to an interface pin
  it so a KOFO module cannot be unloaded while the entity is referenced.
  I/O on these entities goes through the entity arch ops table
  (`entity_arch_io_register`) and the native entity syscalls
  `entityRead`/`entityWrite`/`entitySeek`/`entityIoctl`.
  Nexus creates only generic buses (`firmware`, `platform`, `isa`, `pseudo`);
  PCI creates generic function children; drivers claim devices via probe.
  Resources, IRQs, and timer polling must flow through `bus_set_resource`,
  `bus_alloc_resource*`, `bus_setup_intr`, and `bus_setup_poll`.
  Registry keys under `SYSTEM.Newbus` are a policy/config overlay only:
  they may enable/disable drivers, buses, or devices and provide driver
  tunables, but must not become a concrete driver startup list or device
  discovery source.
  Linker-set registration depends on the `.newbus.drivers` section in
  `src/linker.ld`.
- `drivers/USB/` — USB transport-neutral core.  It owns standard descriptor
  parsing, endpoint contracts and USB-interface newbus children; host
  controllers and class drivers remain separate modules.  USB interfaces are
  dynamically enumerated, so their newbus drivers must support reprobe/hot-plug.
- `drivers/pmu/` — CPU performance monitoring driver. Owns CPUID/MSR PMU
  detection, counter programming, per-CPU counter state, and `drivers_log`
  status lines. Trace code should consume it through the PMU API instead of
  programming processor counters directly.
- `drivers/fs/chainFS/` — custom filesystem used for root disk (512-byte blocks,
  superblock + file table + block map + chained data). Tooling in `chainfs.py`.
- `drivers/fs/hivefs/` — read-only registry hive filesystem. It validates the
  `cmseed` boot module and exposes hives/keys/values as VFS nodes.
- `drivers/fs/vfs/` — tiny vnode-based VFS; used by POSIX personality and devfs.
- `drivers/fs/devfs/` — device nodes: `/dev/null`, `/dev/zero`, `/dev/random`,
  `/dev/urandom`, `/dev/tty`, `/dev/console`, `/dev/fb0`, `/dev/ptmx`,
  `/dev/pts/*`. `/dev/fb0` is a root-only Linux fbdev-compatible character
  device backed by the boot linear framebuffer.
- `drivers/video/drm/` — minimal DRM: GEM buffers, KMS objects, primary/cursor
  planes, atomic commit, fbdev, render helpers (`rapi`), and a virtio-gpu
  backend.
- `drivers/video/evr/` — early video renderer for the Multiboot linear
  framebuffer. It works before DRM/KMS is ready, then stops through
  `evr_handoff()` only after a successful DRM initialization.
- `net/` — polling Ethernet/ARP/IPv4/ICMP/UDP stack with a native TCP stream
  MVP; virtio-net supports modern PCI and legacy/transitional PCI I/O
  transports. RX is polled from timer IRQs after the network subsystem has
  initialized.
- `console/` — kernel terminal, pty, and console rendering.
- `event/` — kqueue-style event system (read, write, timer, proc, signal, user,
  keyboard, mouse, and native IPC filters).
  Positive `kevent` timeouts arm the current thread's `sleep_target_ticks`;
  `time_tick()` wakes the waiter at its deadline.  A timed wait must never call
  `proc_sleep()` without arming and later clearing that deadline.
- `ipc/` — named native IPC services and connected sessions with atomic
  messages, credentials, correlation IDs, bounded queues, backpressure, timed
  calls, handle lifecycle integration, and `EVFILT_IPC` readiness. Messages
  may transfer up to `IPC_MAX_HANDLES` entity capabilities; queued messages
  retain their entities, and receivers get process-local handles preserving
  the sender's access mask.
- `trace/` — kernel observability core: DTrace-like providers/probes, safe
  per-session programs with predicates/actions, per-session per-CPU buffers,
  kernel aggregations, PMU samples from `drivers/pmu`, and syscall/IRQ/
  scheduler/kqueue tracepoints. Runtime toggles live in the `SYSTEM` registry
  hive.
- `entity/` — data-oriented entity manager: `entity.c` (SoA store, IDs,
  refcounts), `entity_handle.c` (per-process handle tables), `entity_ns.c`
  (native `/Entity` namespace), plus `api/entity.c` syscall handlers and
  `api/entity_io.c` (archetype release hooks + IO helpers). Legacy
  `api_handle_t`/`api_object_t` tables are removed: native data/net/ipc/reg
  syscalls and the POSIX fd layer are backed by entity handles. kqueue ids
  and `EVFILT_ENTITY` are entity-aware; SHM segments and trace sessions are
  registered as entities (`ENTITY_ARCH_SHM`, `ENTITY_ARCH_TRACE`). GEM
  buffers, KOFO modules, newbus devices/interfaces, DRM KMS objects, TTYs,
  processes and threads are registered as observable entities
  (`ENTITY_ARCH_GEM`, `ENTITY_ARCH_KOFO`, `ENTITY_ARCH_NB_INTERFACE`,
  `ENTITY_ARCH_NB_DEVICE`, `ENTITY_ARCH_DRM`, `ENTITY_ARCH_TTY`,
  `ENTITY_ARCH_PTY`, `ENTITY_ARCH_PROCESS`, `ENTITY_ARCH_THREAD`). GEM, KOFO
  and newbus handles are entity kernel handles (pid 0) with raw-slot fallback
  before entity init. Handles are `(generation << 16) | slot`; entity IDs
  pack archetype/generation/index. Entity I/O (read/write/seek/ioctl) is
  dispatched through per-archetype `entity_io_ops_t` tables registered with
  `entity_arch_io_register`; `api/entity_io.c` owns the dispatcher and the
  FILE/VNODE/PIPE handlers, `drivers/newbus/interface.c` owns the newbus
  handlers.
  Access policy is runtime-configurable via `SYSTEM.Entity.EnforceOwnership`.
- `kshell/` — optional kernel debug shell; runtime command metadata and prompt
  live in the `SYSTEM` registry hive.
- `crypto/` — SHA-256, HMAC-SHA256, PBKDF2, ChaCha20, RNG, plus `kusr`
  authentication.
- `interrupts/`, `gdt.c`, `time.c`, `futex.c`, `syscall.c`, `process.c`,
  `thread.c`, `scheduler.c`, `panic.c`, `useraddr.c`.
  The interrupt subsystem has a logical IRQ core in `interrupts/irq.c` with
  dynamic vector allocation and PIC, IOAPIC/GSI, and LAPIC-local domains.
  Hardware drivers register through newbus `bus_setup_intr()`; the core owns
  shared-line dispatch, ACPI ISO routing, masking, EOI, statistics, and storm
  handling. Device interrupt delivery must not be added to timer poll hooks.

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
  and a small TOML parser still used by legacy kusr credential storage.
- Simple mlibc routines may be written in Kato and compiled through
  `etc/kato/src/main.py` into generated C objects during the kernel build.

### ELF loader (`src/userland/`)

- `elf.zig` — ELF parsing/loading in Zig.
- `userspace.c` — address-space setup, user stack allocation, and process
  creation.
- `userspace.asm` — assembly to enter Ring 3.

### Userspace programs (`init/`, `progs/`, `ports/`)

- `init/init.c` — first userspace process, event-driven program spawner via
  `procSpawn` and `procWait`.
- `progs/yes.c`, `fetch.c`, `shell.c`, `udp_echo.c`, `tcp_echo.c`, `send.c`,
  `ssh/main.c`, `posix_hello.c`, `musl_test.c`, `demons/cursord.c` — small test/utility
  binaries and daemons. `dhcpc` is an IPC control client; `dhcpd` owns the
  DHCP lease lifecycle and publishes `system.network.dhcpd`.
- `progs/posix_hello.c` uses Linux x86-64 syscall numbers and expects the
  POSIX personality.
- `progs/musl_test.c` uses real musl headers and is statically linked against
  vendored musl.
- `progs/yabox/` contains native small utility programs such as `ls`, `cat`,
  `cp`, `mv`, `rm`, `mkdir`, `unzip`, and `profile`.
- `progs/toolchain/` contains native OTSOS toolchain programs such as `as` and
  `ld`.
- `progs/regedit/` is the registry editor.
- `ports/lua/` — Lua interpreter port, uses musl's `Scrt1.o` and is dynamically
  linked as a PIE against shared `libc.so` (via `-pie -dynamic-linker /lib/ld-musl-x86_64.so.1`).
  Patched via `ports/lua/diff.patch`, set up by `ports/lua/setup.sh`.

### Vendored libc (`libc/musl/`)

- Configured for x86_64-linux-musl with shared libraries enabled.
- Builds both `lib/libc.a` (static) and `lib/libc.so` (shared + dynamic linker).
- The dynamic linker (`ld-musl-x86_64.so.1`) is `libc.so` itself (musl unified model).
- `lib/Scrt1.o` — PIE startup crt, used by dynamically linked programs.
- The kernel build optionally links `progs/musl_test` statically against it.
  Do not modify upstream musl unless you know what you are doing.

### Native libc (`libc/native/`)

- Native C runtime for the native userspace ABI, separate from musl and separate from
  POSIX compatibility.
- Public API uses kernel-native names: `term*`, `data*`, `fs*`, `proc*`,
  `event*`, `sys*`, `drm*`.
- Source files are split by subsystem (`data.c`, `fs.c`, `proc.c`,
  `event.c`, `sys.c`, `drm.c`, etc.); do not collapse them into one wrapper
  file.
- Native terminal mode control is exposed through `CALL_TERM_MODE` and
  `termMode()`/`termGetMode()`/`termSetMode()`/`termEnterRaw()`/
  `termRestoreMode()`.  Interactive native programs such as SSH should save the
  current `struct api_term_mode`, enter raw mode for byte-granular terminal IO,
  and restore the saved mode before exit.
- Native shared memory uses process-local entity handles. `shmGet()` creates or
  opens a segment, `shmMap()` maps it, `shmCtl()` provides `STAT`/`RMID`, and
  `shmClose()` releases the handle. SHM handles can be transferred through
  native IPC capability slots; raw SysV SHM ids remain internal to the POSIX
  personality.
- `stdio` is buffered
- Builds `lib/crt0.o` and `lib/libc.a` for freestanding userspace binaries.
- Keep POSIX-only work on musl / personality layer; do not blur the two.

### Native archive library (`libc/LibArchive`)

- Small native userspace archive library built on top of `libc/native`.
- Currently exports `la_zip_extract()` for ZIP extraction.  Supported ZIP
  method is stored/no-compression only;

### Native crypto library (`libc/LibCrypto`)

- Small native userspace crypto library built on top of `libc/native`, not
  POSIX/OpenSSL.
- Builds `lib/libcrypto.a` and exports a compact SSH-oriented primitive set:
  random bytes, secure wipe, constant-time memory equality, SHA-256, SHA-512,
  HMAC-SHA256, IETF ChaCha20, original 64-bit nonce/counter ChaCha20 for SSH
  packet ciphers, Poly1305, RFC8439 ChaCha20-Poly1305, Curve25519/X25519, and
  Ed25519 signature verification.
- Poly1305 is used by the SSH transport MAC path; keep its final
  reduction/serialization compatible with RFC Poly1305 vectors before
  debugging higher-level SSH packet authentication failures.
- Keep new primitives freestanding and native-ABI friendly.  Do not add POSIX
  file/socket dependencies here.

### Native SSH library (`libc/LibSSH`)

- Native userspace SSH protocol library built on top of `libc/native` and
  `libc/LibCrypto`; it is intended to support a real native SSH client rather
  than a POSIX port.

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

- `src/config.toml` — build-time Makefile input for feature toggles and the
  version bump prompt. The kernel does not load it as runtime config.
- `config/hives/*.hive` — source DSL for initial registry hives.  These files
  are compiled by `tools/hivec.py` into `../bin/cmseed`, which is copied into
  the bootpack as module `cmseed`.  The kernel loads this module into the
  read-only `hivefs` backend. `kernel/cm/` mounts a kusr-only debug view at
  `/conf/registry` and reads runtime settings through `hivefs` vnodes.
- `BOOT.Modules.*.Dest` in `config/hives/BOOT.hive` maps Multiboot module
  names to filesystem destinations. The Makefile asks `tools/hivec.py` for the
  module names when building the bootpack, and the kernel reads the same keys
  at runtime to install modules into ChainFS.
- The `ld-musl` module in `[modules]` provides the musl dynamic linker at
  `/lib/ld-musl-x86_64.so.1` for dynamically linked programs.

### Tooling / scripts

- `chainfs.py` — inspect/format/mount ChainFS images from the host.
- `add_copyright.sh` — prepends the BSD-2-Clause copyright header to `.c`,
  `.h`, `.s` files.
- `tools/toml_get.sh` — helper used by Makefiles to read `config.toml` values.
- `tools/modules.pl` — legacy extractor for old `[modules]` TOML configs; do
  not use it for new bootpack module wiring.
- `tools/hivec.py` — compiles registry hive DSL files into the binary `cmseed`
  hive pack.  The format is little-endian and table-based: HPK contains HIVE
  blobs, and each HIVE contains node/value/string/data tables plus CRC32.
- `tools/makeiso.py` — small `xorriso` wrapper that creates the hybrid
  BIOS+UEFI ISO from the BIOS disk image and EFI system partition image.

## Repository Structure

- `src/` — kernel, bootloader, mlibc, userland ELF loader.
- `src/kernel/cm/` — configuration manager over `hivefs`.
- `config/hives/` — source registry hive DSL used to build the initial
  `cmseed` boot module.
- `init/` — first userspace process (`init`) and `hello` test.
- `progs/` — native OTSOS userspace programs and native toolchain utilities.
- `ports/` — third-party ports such as Lua and CPython.
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
- `src/kernel/drivers/newbus/` — driver module registry, generic bus tree,
  resource tables, IRQ dispatch, and poll hooks.
- `src/userland/userspace.c:56` — `userspace_init()` — enable Ring 3.
- `src/userland/userspace.c:139` — `userspace_load_elf()` — create a process.
- `init/init.c:250` — `_start()` — first userspace process.
- `src/kernel/scheduler.c` — `scheduler_tick()` — timer-driven preemption.
- `src/kernel/trace/trace.c` — `trace_init()` — profiling/tracing core.
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

- Virtio PCI common configuration 64-bit queue addresses are written as two
  32-bit accesses. VirtualBox rejects or mishandles a single 64-bit MMIO store
  even though QEMU accepts it.
- Do not touch `kernel.c` boot order without understanding the memory-map and
  Multiboot module layout. Driver startup belongs in newbus modules, not in
  `kernel.c`.
- Do not add concrete driver calls or lists to `nexus.c`, `kernel.c`, IRQ
  handlers, PCI core, DRM core, VFS core, or DevFS init. Add or update a module
  in the driver implementation and let it probe through the generic bus/resource
  APIs.
- Do not derive interrupt vectors from IRQ or GSI numbers. Obtain vectors from
  the IRQ core; vector 48 is reserved for the LAPIC timer, 128 for `int 0x80`,
  and 255 for LAPIC spurious interrupts.
- ChainFS is a flat, single-chain filesystem; it is not POSIX-safe for hard
  links or large files. POSIX personality uses the VFS and devfs on top of it.
- POSIX personality is opt-in per process via `CALL_PERSONALITY` (`0xFFFF`);
  default is native `PERSONALITY_OTSOS`.
-Newbus devices/interfaces are entities
  bound under `/Entity/Interface/Driver/...`; do not mount them in VFS and do
  not expose them to POSIX syscalls. POSIX programs use `/dev`.
- `/Entity` is the native-ABI-only entity namespace; it is not mounted in VFS
  and must stay invisible to the POSIX personality. Entity syscalls live in
  the `0xD00` block and are implemented in `kernel/api/entity.c`.
- Do not reintroduce per-subsystem handle/object tables; new resources must
  register an archetype and go through `entity_io_*` / `entity_handle_*`.
- POSIX `mremap(2)` is implemented in `api/posix/posix_mem.c` on top of
  VMA clipping/relocation helpers in `mm/vm/vm_map.c`; `MREMAP_DONTUNMAP`
  deliberately returns `EINVAL` because its Linux userfaultfd semantics are not
  modeled yet.
- `src/config.toml` is build-time only. Runtime kernel settings must go through
  `kernel/cm/` and the registry hives.
- Console policy lives under `SYSTEM.Console`: `DefaultTty` and
  `KernelLogTty` are terminal indices 0-9, `DefaultColor` is a VGA attribute,
  and `MouseBlinkMs` is validated to 50-5000 milliseconds. The console
  consumer is applied after CM is mounted and can be refreshed with
  `API_REG_CONSUMER_CONSOLE`.
- Keyboard policy lives under `SYSTEM.Input.Keyboard.PreferredDriver` and
  accepts `auto`, `ps2`, or `usb`. The input consumer switches only registered
  drivers; hardware sources continue to be drained while inactive without
  publishing their events. Refresh it with `API_REG_CONSUMER_INPUT`.
- Relative mouse hardware publishes `dx`/`dy`; display-space pointer position
  belongs to the compositor, which integrates and clamps those deltas to its
  output.  Sprot pointer coordinates are absolute within the target surface
  and DE/LibG marks them with `SRAPI_MOUSE_ABSOLUTE`.  Do not make the kernel
  input layer depend on DRM dimensions.
- Network stack policy lives under `NETWORK.Stack`: `Enabled`, `PollHz`
  (1-1000),
  and `DefaultTtl` (1-255). Interface keys may set `Mtu`; Ethernet interfaces
  are limited to IPv4 minimum MTU 68 and the lower of 1500 or the driver's
  advertised maximum.
  Network settings are cached by `CM_CONSUMER_NET` and never read from poll or
  packet hot paths.
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
- `kernel.c` → memory (`mm/*`), interrupts (`idt`), timer core, newbus,
  userspace, kshell, CM registry, crypto, syscalls. Concrete drivers attach via
  newbus passes instead of direct boot calls.
- `syscall.c` → `api/*` handlers; for `PERSONALITY_POSIX` it delegates to
  `posix_syscall_handler()`.
- `process.c`/`thread.c`/`scheduler.c` → each other, `mm/vm/pmap.h`, `idt.h`.
- `userspace.c` → `elf.zig`, `pmap`, `vm_page`, `vm_map`, `vm_object`,
  `process`, `thread`, `gdt`.
- `api` layer → VFS/devfs/ChainFS, terminal/DRM, process/thread, memory, time.
- `drivers/fs/vfs` exposes backend registration; concrete filesystem/devfs
  modules register themselves through newbus.
- `drivers/video/drm` exposes driver registration; display backends register
  themselves through newbus or their parent bus.
- `kshell` → `console`, `terminal`, `keyboard`, `drm`, `cm`.
- `crypto` → `drivers/timer` for RNG entropy; `kusr` → `crypto` for
  PBKDF2/HMAC auth.


## Things Every Agent Should Know

- Kernel data allocations (bootmem, kmem heap, vm_page metadata, GEM
  buffers) are accessed through the direct map at `DMAP_BASE`
  (`0xFFFF800000000000`), which maps physical memory 0-64GB with 2MB huge
  pages; it is built statically in `src/boot/boot.s` (PML4[256]) before long
  mode. `KERNEL_VMA` (`0xFFFFFFFF80000000`) is only the kernel image mapping
  (phys 0-2GB). Phys->VA for kernel data: `phys + DMAP_BASE`; VA->phys:
  `va - DMAP_BASE`. Kernel image addresses (e.g. `&kernel_end`) still use
  `KERNEL_VMA`. `pmap_table_ptr()`, `bootmem_alloc()`, kmem growth, physical
  page zero/copy helpers and the Zig ELF loader's `phys_to_ptr()`
  (`src/userland/elf.zig`) must all use `DMAP_BASE`; the old
  `KERNEL_VMA`-based data mapping wraps for phys >= 2GB and silently
  aliases the identity map on machines with more than 2GB RAM.
- The entity store uses an AoSoA layout: metadata is SoA columns inside
  blocks of `ENTITY_BLOCK_ENTRIES`
- otsos2 is a single-address-space kernel during boot; userspace processes get
  their own page tables via `pmap_create()`.
- The kernel is loaded at `0xFFFFFFFF80000000` (higher half) after the bootloader
  sets up identity + higher-half mappings. See `src/linker.ld`.
- Userspace programs are loaded at `0x400000` and use a stack at
  `0x00007FFFFFFFFFF0`. See `init/linker.ld`.
- The kernel.c copies Multiboot modules (`init`, `yes`, `sh`, `fetch`,
  `posix_hello`, `send`, optional `musl_test`) into `/bin/` on the root
  filesystem before starting `init`. These are not on disk; they come from the
  ISO.
- The bootpack also carries `cmseed`, generated from `config/hives/*.hive`.
  The `hivefs_cmseed` newbus firmware module validates it, initializes
  `kernel/cm/` early, and later mounts `/conf/registry` as read-only, noexec,
  nodev, kusr-only.
- `config.toml` changes can affect build-time feature flags, but not runtime
  kernel settings or bootpack module destinations.
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
