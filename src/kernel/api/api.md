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
- `drmCall(op, arg)` low-level DRM control.

`drmCall` dispatches on `op`. Read-only queries are available to every
process; screen-mutating operations require kusr rights (else `-PERM`).

Public (no kusr):
- `op=DRM_OP_INFO`        fill `struct api_drm_info` at `arg`
- `op=DRM_OP_DRIVER_NAME` copy active driver name (32 bytes) to `arg`

kusr only:
- `op=DRM_OP_CLEAR`       `arg` points to a `u32` color
- `op=DRM_OP_PUT_CHAR`    `arg` points to `struct api_drm_char`
- `op=DRM_OP_PUT_PIXEL`   `arg` points to `struct api_drm_pixel`
- `op=DRM_OP_FILL_RECT`   `arg` points to `struct api_drm_rect`
- `op=DRM_OP_SCROLL`      `arg` points to an `s32` line count
- `op=DRM_OP_BATCH_BEGIN` `arg` ignored
- `op=DRM_OP_BATCH_END`   `arg` ignored
- `op=DRM_OP_FLUSH`       `arg` ignored

All ops return 0 on success or a negative error code on failure.

## Data Model

`term*` calls are separate from file/data handles. ChainFS files and pipes use
small integer handles backed by shared kernel objects. `procSpawn` creates a new
process from a ChainFS ELF image and returns its PID.

The old device namespace is not created by the kernel and data handles reject
device paths. Console I/O goes through `termRead` and `termWrite`.
