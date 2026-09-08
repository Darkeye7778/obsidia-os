#include "syscall.h"
#include <obsidia/system.h>

int os_system_control(obs_system_action_t action){
    int64_t handle=os_handle_find(OS_OBJECT_SYSTEM_CONTROL);if(handle<0)return-1;
    return(int)os_system_control_raw((uint64_t)handle,(uint32_t)action);
}
