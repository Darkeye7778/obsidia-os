#include "syscall.h"
#include <obsidia/service.h>
int obsidia_main(void){
    int64_t endpoint=os_ipc_create();if(endpoint<0||os_service_register("display",(uint64_t)endpoint)<0)return 1;
    static const char ready[]="displayd: registered display service\n";sys_fd_write(1,ready,sizeof(ready)-1);
    for(;;){char message[64];if(os_ipc_recv((uint64_t)endpoint,message,sizeof(message))>0){
        static const char connected[]="displayd: desktop session connected\n";sys_fd_write(1,connected,sizeof(connected)-1);
    }}
}
