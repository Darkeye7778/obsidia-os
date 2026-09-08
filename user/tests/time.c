#include <obsidia/time.h>
#include "syscall.h"

int obsidia_main(void){
    uint64_t resolution=obs_time_monotonic_resolution_ns();
    if(!resolution||resolution>10000000ULL)return 3;
    uint64_t first=obs_time_monotonic_ns();
    uint64_t previous=first;
    uint32_t changes=0;
    for(uint32_t i=0;i<10000;i++){
        uint64_t now=obs_time_monotonic_ns();
        if(now<previous)return 1;
        if(now>previous)changes++;
        previous=now;
    }
    if(resolution<1000000ULL&&!changes)return 4;
    sys_sleep(2);
    uint64_t last=obs_time_monotonic_ns();
    uint64_t elapsed=last-first;
    /* Two 100 Hz scheduler ticks should be near 20 ms.  The broad bounds also
       validate the PIT fallback on firmware without HPET. */
    if(elapsed<10000000ULL||elapsed>500000000ULL)return 2;
    uint64_t wall_first=obs_time_wall_utc();
    /* 2020-01-01 through 2100-01-01: rejects missing, nonsensical, and
       accidentally boot-relative values without depending on the host date. */
    if(wall_first<1577836800ULL||wall_first>=4102444800ULL)return 5;
    sys_sleep(110);
    uint64_t wall_last=obs_time_wall_utc();
    if(wall_last<wall_first||wall_last-wall_first>5)return 6;
    static const char passed[]="TIME: monotonic clock regression passed\n";
    sys_fd_write(1,passed,sizeof(passed)-1);
    return 0;
}
