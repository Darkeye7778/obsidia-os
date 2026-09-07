#include "syscall.h"
#include <obsidia/window.h>
#include <obsidia/service.h>
#include <obsidia/internal/display_protocol.h>
int obsidia_main(void){
    static const char start[]="window-negative: start\n";sys_fd_write(1,start,sizeof(start)-1);
    obs_window_t window;if(os_window_create(&window,0,300,"invalid")>=0)return 1;
    if(os_window_create(&window,119,80,"too narrow")>=0||os_window_create(&window,120,79,"too short")>=0)return 1;
    int64_t display=os_service_connect("display"),reply=os_ipc_create();if(display<0||reply<0)return 2;
    obs_display_create_request_t bad={0};bad.operation=0xdeadbeefU;
    if(os_ipc_send_handle((uint64_t)display,&bad,sizeof(bad),(uint64_t)reply,OS_RIGHT_WRITE|OS_RIGHT_DUP)!=(int64_t)sizeof(bad))return 3;
    obs_display_create_response_t response;int64_t attached=-1;int64_t received=os_ipc_recv_handle((uint64_t)reply,&response,sizeof(response),&attached);
    if(received!=-3||os_ipc_recv((uint64_t)reply,&response,sizeof(response))!=(int64_t)sizeof(response)||response.status>=0)return 4;
    static const char passed[]="window-negative: invalid dimensions/operation rejected\n";sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
