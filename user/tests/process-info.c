#include "syscall.h"
#include <obsidia/app.h>
#include <obsidia/process.h>

static int same(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
int obsidia_main(void){
    obs_process_info_t self={0};uint64_t pid=sys_getpid();
    if(os_process_info(pid,&self)<0||self.pid!=pid||!same(self.image,"process-info.elf")||self.image[31])return 1;
    if(os_process_info(0,&self)==0||os_process_info(0xffffffffffffffffULL,&self)==0)return 2;
    int64_t child=obs_app_launch("process-child" OBS_NATIVE_EXEC_EXTENSION);if(child<0)return 3;
    obs_process_info_t info={0};if(os_process_info((uint64_t)child,&info)<0||info.pid!=(uint64_t)child||info.parent_pid!=pid||!same(info.image,"process-child.obsx"))return 4;
    int64_t status=0;if(sys_wait((uint64_t)child,&status)<0||status!=7)return 5;
    if(os_process_info((uint64_t)child,&info)==0)return 6;
    static const char passed[]="process-info: current/other/dead/invalid bounded queries passed\n";sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
