#include "syscall.h"
#include <obsidia/window.h>

int obsidia_main(void){
    int64_t control=os_handle_find(OS_OBJECT_IPC);if(control<0)return 1;
    obs_window_t window;if(os_window_create(&window,160,90,"Abrupt owner")<0)return 2;
    for(uint32_t i=0;i<window.width*window.height;i++)window.pixels[i]=0x00473b68;
    os_window_present(&window);
    uint32_t id=window.id;if(os_ipc_send((uint64_t)control,&id,sizeof(id))!=(int64_t)sizeof(id))return 3;
    sys_sleep(5);uint8_t command=0;int64_t ack=-1;
    int64_t received=os_ipc_recv_handle((uint64_t)control,&command,1,&ack);
    if(received==-3)received=os_ipc_recv((uint64_t)control,&command,1);
    if(received!=1)return 4;
    if(command==2)__asm__ volatile("ud2");
    if(command==4){os_window_resize(&window,320,180);return 0;}
    if(command==5){os_window_resize(&window,320,180);__asm__ volatile("ud2");}
    if(command==6){os_window_minimize(&window);return 0;}
    if(command==3){
        os_handle_close(window.session_handle);window.session_handle=(uint64_t)-1;
        static const char detached[]="window-abrupt: session closed while owner remained alive\n";
        sys_fd_write(1,detached,sizeof(detached)-1);sys_sleep(5);
        uint8_t detached_ack=1;if(ack<0||os_ipc_send((uint64_t)ack,&detached_ack,1)!=1)return 6;
        os_handle_close((uint64_t)ack);
        sys_sleep(45);return 0;
    }
    static const char exiting[]="window-abrupt: exiting without close\n";sys_fd_write(1,exiting,sizeof(exiting)-1);
    return command==1?0:5;
}
