drld = dynamic runtime loader


amd64(x86-64) ELF64 only
native libc + native kernel API only
intended as PT_INTERP loaded at 0x40000000
loads DT_NEEDED shared objects from /lib, /usr/lib, or direct paths
support SYSV and GNU hash symbol lookup
support RELA relocations RELATIVE, GLOB_DAT, JUMP_SLOT, COPY, 64, PC32,
  32, 32S, IRELATIVE
didnt support tls symbol versioning lazy binding dlopen/dlsym and
  full RELRO/mprotect
