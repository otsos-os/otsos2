# Kernel API

This layer is the OTSOS data-oriented userspace API. It is not POSIX and it
does not expose terminal devices through a filesystem namespace.

## Calls

Terminal:
- `termRead(buf, count)` reads from the active terminal.
- `termWrite(buf, count)` writes to the active terminal.

Data handles:
- `dataOpen(path, flags) -> handle`
- `dataClose(handle)`
- `dataRead(handle, buf, count)`
- `dataWrite(handle, buf, count)`
- `dataSeek(handle, offset, whence)`
- `dataPipe(handles[2])`

Process:
- `procCopy()`
- `procSpawn(path, argv, envp)`
- `procExit(code)`
- `procWait(status)`
- `procKill(pid, sig)`

Other:
- `memMap(args)`
- `sysInfo(buf)`
- `drmCall(op, arg)` is reserved for low-level DRM control.

## Data Model

`term*` calls are separate from file/data handles. ChainFS files and pipes use
small integer handles backed by shared kernel objects. `procSpawn` creates a new
process from a ChainFS ELF image and returns its PID.

The old device namespace is not created by the kernel and data handles reject
device paths. Console I/O goes through `termRead` and `termWrite`.
