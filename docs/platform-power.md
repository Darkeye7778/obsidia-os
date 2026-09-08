# Platform discovery and power control

The kernel validates the bootloader-provided ACPI RSDP, RSDT/XSDT, child-table
checksums, FADT, and the DSDT `_S5` package before retaining bounded power-control
metadata. Physical table addresses are checked against the firmware memory map.
Shutdown uses the ACPI PM1 control blocks; reboot prefers the FADT reset register
and has the architecture-standard i8042 reset as a fallback.

Userspace requests power operations through a generic system-control capability.
The initial `init` process receives it, ordinary children do not, and a future
session/power service may receive an explicitly inherited reduced authority.
Before either operation, registered physical block devices are flushed. Themes,
shell behavior, and power-menu policy remain entirely outside the kernel.

Current scope is ACPI table discovery, S5 shutdown, and reset on the uniprocessor
PC platform. AML interpretation is intentionally limited to the standardized S5
object; suspend, batteries, thermal policy, SCI events, and full AML execution
remain future platform milestones.
