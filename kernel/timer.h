#pragma once
#include <stdint.h>

void timer_init(void);
uint64_t timer_get_ticks(void);
void timer_sleep(uint64_t ticks);  // busy-wait sleep using hlt for base
