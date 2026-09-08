#pragma once
#include <stdint.h>

int rtc_init(void);
int rtc_read_utc_seconds(uint64_t*seconds);

