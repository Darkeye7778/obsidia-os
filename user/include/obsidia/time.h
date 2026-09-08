#pragma once
#include <stdint.h>

/* Nanoseconds since an unspecified boot-local epoch.  This clock never
   represents wall time and is suitable for durations and event ordering. */
uint64_t obs_time_monotonic_ns(void);
uint64_t obs_time_monotonic_resolution_ns(void);

/* Whole seconds since 1970-01-01 00:00:00 UTC, or UINT64_MAX when the
   platform has no usable real-time clock. */
uint64_t obs_time_wall_utc(void);
