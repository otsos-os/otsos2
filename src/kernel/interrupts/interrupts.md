# Interrupt Management

OTSOS uses a unified IRQ core between x86 interrupt entry, interrupt
controllers, newbus resources, and device drivers.

## Entry And Vectors

`idt.asm` generates hardware stubs for vectors 32 through 254. `idt.c` installs
them from `irq_vector_stubs`; exceptions and the user `int 0x80` gate remain
separate.

The IRQ core allocates vectors dynamically. It permanently reserves vector 128
for the syscall gate, vector 48 for the LAPIC timer, and vector 255 for LAPIC
spurious interrupts. Dispatch uses a vector-to-descriptor table and never
assumes `vector == irq + 32`.

## Sources And Domains

An `irq_source_t` identifies an interrupt by domain, hardware IRQ, electrical
attributes, and allocated vector. Supported domains are:

- PIC legacy IRQs;
- IOAPIC GSIs;
- LAPIC local interrupts.

ISA sources pass through ACPI MADT Interrupt Source Override resolution before
registration. Overrides supply the GSI, polarity, and trigger mode. All MADT
IOAPIC entries are discovered and mapped, so routing is selected by GSI range
rather than by a fixed 24-entry controller.

PIC is used when IOAPIC is unavailable. IRQ7 and IRQ15 use PIC ISR checks for
correct spurious interrupt handling.

## Actions And Lifecycle

`irq_request()` creates or joins a descriptor and `irq_release()` removes an
action. Shared registration requires every action to opt in and requires
compatible trigger, polarity, and per-CPU attributes.

The first action routes and unmasks the source. Removing the last action masks
the source, removes the vector mapping, and releases dynamically allocated
vectors. IOAPIC and LAPIC interrupts receive LAPIC EOI; PIC sources receive PIC
EOI.

Each descriptor tracks total, handled, and unhandled interrupts. Repeated
unhandled interrupts are rate-limited in logs and a sustained unhandled storm
masks non-per-CPU sources. `irq_stats_dump()` prints descriptor state and
counters.

## Newbus

Drivers obtain active `SYS_RES_IRQ` resources and register through
`bus_setup_intr()`. Resource flags describe ISA/GSI namespace, sharing,
trigger mode, and polarity. Newbus converts the resource to `irq_source_t` and
adapts the existing driver callback result to `IRQ_HANDLED` or `IRQ_NONE`.

Drivers must disable device interrupt generation, call `bus_teardown_intr()`,
then release the IRQ resource before destroying device state.

## Consumers

PIT and LAPIC timer register the same system tick action. Selecting LAPIC timer
releases and masks the PIT IRQ source, preventing duplicate ticks.

PS/2 keyboard and mouse, UART RX, virtio-net INTx, EHCI, OHCI, and xHCI use
newbus IRQ resources. Their former timer polling paths and private IRQ
registries are not part of interrupt delivery. Protocol timers, watchdog
servicing, and software recovery work remain timer events because they are not
hardware interrupt sources.
