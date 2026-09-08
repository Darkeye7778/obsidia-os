#include <obsidia/process.h>
#include "syscall.h"

int os_process_info(uint64_t pid,obs_process_info_t* info){
    if(!pid||!info)return -1;
    return (int64_t)syscall(SYS_PROCESS_INFO,pid,(uint64_t)info,0)<0?-1:0;
}
