#include "syscall.h"
#include <obsidia/system.h>

int obsidia_main(void){
    /* Normal children do not inherit init's system-control capability. */
    if(os_handle_find(OS_OBJECT_SYSTEM_CONTROL)>=0||os_system_control(OBS_SYSTEM_POWER_OFF)==0||os_system_control(OBS_SYSTEM_REBOOT)==0)__asm__ volatile("ud2");
    static const char passed[]="system-control: unprivileged power operations rejected\n";
    sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
