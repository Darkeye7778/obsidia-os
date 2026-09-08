#pragma once
#include <stdint.h>

/* HPET is the high-resolution monotonic clock.  The scheduler intentionally
   remains driven by the PIT until a separate clock-event abstraction exists. */
int hpet_init(void);
int hpet_is_available(void);
uint64_t hpet_monotonic_ns(void);
uint64_t hpet_resolution_ns(void);
