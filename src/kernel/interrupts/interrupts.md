# Interrupt Management

OTSOS handles hardware and software interrupts using the x86_64 Interrupt Descriptor Table (IDT).

## 1. IDT (Interrupt Descriptor Table)
The IDT contains 256 entries (gates). Each entry points to a kernel function (stub) that handles a specific interrupt.

### Gate Structure
- **Offset**: Address of the handler.
- **Selector**: Kernel code segment selector (0x08).
- **Type**: 0x8E (Interrupt Gate, Present, Ring 0).

## 2. Interrupt Stubs
Interrupt handlers are divided into two parts:
1. **Low-level Stubs (`idt.asm`)**: ASM wrappers that save registers (`pushaq`), call the C handler, and restore registers.
2. **High-level Handlers (`handlers.c` / `idt.c`)**: C functions that implement the logic.

## 3. Categories of Interrupts

### Exceptions (0-31)
Handled by `isr_handler`. Includes:
- `0`: Division by Zero
- `13`: General Protection Fault
- `14`: Page Fault

### IRQs (Hardware Interrupts) (32-47)
The PIC is remapped to offsets `0x20` (Master) and `0x28` (Slave).
- `32`: System Timer
- `33`: Keyboard

## 4. API

### `void init_idt()`
Initializes the IDT, sets up entry points for all 256 interrupts, remaps the PIC, and enables interrupts via `sti`.

### `registers_t`
A structure containing the CPU state at the time of the interrupt, passed to C handlers.
```c
typedef struct {
  unsigned long long r15, r14, r13, r12, r11, r10, r9, r8;
  unsigned long long rbp, rdi, rsi, rdx, rcx, rbx, rax;
  unsigned long long int_no, err_code;
  unsigned long long rip, cs, rflags, rsp, ss;
} registers_t;
```

