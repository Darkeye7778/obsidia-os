# AHCI storage v1

The AHCI driver binds a PCI class `01/06/01` controller, enables memory and bus
mastering, maps its ABAR through the generic cache-disabled MMIO mapper, performs
BIOS/OS ownership handoff when advertised, and discovers implemented SATA ports.
Each accepted port becomes a normal block device after ATA IDENTIFY capacity
validation.

Commands use controller-owned DMA structures and a bounded 64 KiB coherent
bounce buffer allocated through the generic DMA API. The v1 path serializes one
command slot and supports LBA48 READ DMA EXT, WRITE DMA EXT, FLUSH CACHE EXT, and
IDENTIFY. Completion prefers a validated PCI MSI capability and a dynamically
allocated hardware vector. It falls back to the controller's firmware-assigned
PCI INTx line through the shared interrupt-handler and IOAPIC/PIC layers, then to
bounded polling when no usable IRQ exists. The interruptible wait remains safe
when storage is entered through the interrupt-gate syscall path. Block and partition layers
provide additional range validation. DMA storage is page-aligned, zeroed,
physically contiguous, constrained below a caller-specified device address
ceiling, and explicitly reclaimable.

The driver intentionally coexists with the ATA/PIO bootstrap path. Filesystem
selection searches physical block devices and mounts the first validated Obsidia
state partition; execution is not tied to a controller type. `make test-run-ahci`
provides the deterministic QEMU hardware path.

ATA IDENTIFY model and serial fields are normalized into the generic block-device
record.  The block layer also maintains bounded read/write error and timeout
counters plus online state.  On a failed AHCI command the driver performs one
bounded stop, COMRESET, restart, and retry sequence; a second failure marks the
device offline instead of retrying forever.  These diagnostics are kernel-owned
mechanism data and are not yet a userspace device-manager interface.

Current limitations are explicit: one controller, four disks, a synchronous
single-command block API, no NCQ, no hotplug, no ATAPI, no fault-injection coverage
for recovery, and no IOMMU isolation. Those limitations keep AHCI at partial
commercial parity even though interrupt-driven persistent read/write/flush
behavior is operational.
