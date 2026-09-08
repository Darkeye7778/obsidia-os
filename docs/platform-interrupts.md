# Platform interrupt routing

Obsidia validates the ACPI MADT during early firmware discovery and retains only
bounded local-APIC, IOAPIC, and ISA interrupt-override metadata. After the kernel
page tables are active, the interrupt-controller layer maps the xAPIC and IOAPIC
register pages as cache-disabled, masks all IOAPIC inputs, disables the legacy
8259 PIC, and routes only IRQs explicitly requested by a driver.

Legacy ISA IRQ requests are translated through MADT source overrides, including
polarity and trigger mode. The PIT, PS/2 keyboard, and PS/2 mouse therefore keep
their existing device-facing IRQ numbers while the platform layer selects the
actual GSI and destination APIC. Interrupt completion uses local-APIC EOI. Every
x86 interrupt vector has a correctly numbered IDT stub; vector 255 is reserved
for local-APIC spurious interrupts.

PCI devices use a separate INTx routing entry point so their conventional
active-low, level-triggered electrical semantics are never confused with ISA
defaults. The current PC implementation consumes the firmware-populated PCI
configuration interrupt line as its GSI. The PCI layer also walks bounded,
cycle-checked conventional capability lists and can program single-vector MSI to
a dynamically allocated local-APIC vector. ACPI PCI routing tables and MSI-X
remain future extensions.

If the MADT, CPU APIC feature, local APIC, or every IOAPIC is unavailable, setup
fails closed and the same driver API falls back to the remapped 8259 PIC. QEMU
regression covers both paths. This v1 implementation is uniprocessor xAPIC only;
x2APIC, SMP targeting, MSI-X, full ACPI PCI routing policy, interrupt balancing, and
hotplug remain future work.
