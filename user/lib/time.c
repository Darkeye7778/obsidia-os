#include <obsidia/time.h>
#include "syscall.h"

uint64_t obs_time_monotonic_ns(void){return sys_time_monotonic_ns();}
uint64_t obs_time_monotonic_resolution_ns(void){return sys_time_resolution_ns();}
uint64_t obs_time_wall_utc(void){return sys_time_wall_utc();}
