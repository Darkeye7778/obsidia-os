# Platform time

Obsidia discovers the ACPI HPET table, validates its memory Generic Address
Structure, maps the timer as kernel MMIO, and enables its free-running counter.
The timer is used as a high-resolution monotonic clock; PIT IRQ0 remains the
scheduler clock-event source for now. This deliberately separates timekeeping
from scheduling policy.

Native applications use `obs_time_monotonic_ns()` for durations and event
ordering, and can query `obs_time_monotonic_resolution_ns()`. The epoch is
unspecified and is not wall-clock time. Machines without HPET retain a 10 ms PIT
fallback. Both paths use the same ABI. A 32-bit HPET counter is extended at each
scheduler tick, while a 64-bit counter is read directly.

RTC wall time, time zones, calendar conversion, and network synchronization are
separate from the monotonic source. The kernel also performs bounded, stable
double-sampling of the CMOS RTC, handles BCD/binary and 12/24-hour modes, uses
the ACPI century register when available, and exposes whole UTC epoch seconds
through `obs_time_wall_utc()`. Locale formatting, time zones, clock adjustment,
and network synchronization remain userspace-service work.
