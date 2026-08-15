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

## Input Events

Native userspace receives direct input through kqueue `EVFILT_INPUT`.  The event
payload is `struct api_input_event` in the trailing `input` field of
`struct kevent`.

Mouse events use `type == API_INPUT_TYPE_MOUSE` and carry both accumulated
device coordinates (`x`, `y`) and packet deltas (`dx`, `dy`, `dz`).  Button
state is in `buttons`; movement/button/wheel/overflow state is in `flags`.

Keyboard events use `type == API_INPUT_TYPE_KEYBOARD` and carry normalized key
data in `key`, `raw`, `mods`, and `ch`.

If the shared input ring overflows for a subscriber, `flags` contains
`API_INPUT_FLAG_DROPPED` and `lost` reports how many events were skipped.

## Power Events

Native userspace receives ACPI power-button notifications through kqueue
`EVFILT_POWER` with `ident == POWER_EVENT_IDENT_SYSTEM`. A button event has
`fflags & NOTE_POWER_BUTTON` and `data` contains the number of button presses
since that knote last reported an event. Register with `EV_ADD | EV_CLEAR`.

The privileged `CALL_POWER_STATE` syscall takes one argument: use
`API_POWER_STATE_SHUTDOWN` or `API_POWER_STATE_REBOOT`. The syscall returns
`-API_ERR_PERM` unless the calling process has kusr privilege; successful
shutdown and reboot requests do not return.
