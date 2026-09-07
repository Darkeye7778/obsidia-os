#include "syscall.h"
#include <obsidia/service.h>
#include <obsidia/input.h>
#include <obsidia/internal/display_protocol.h>

/* Keep motion compact between the kernel input queue and displayd.  A button or
 * key is an ordering barrier: motion is always flushed before it. */
static int32_t signed_reserved(uint32_t value){
    int64_t wide=value;
    return wide>=0x80000000LL?(int32_t)(wide-0x100000000LL):(int32_t)wide;
}
static int send_event(int64_t display,const obs_input_event_t*event){
    obs_display_input_request_t request={OBS_DISPLAY_FORWARD_INPUT,*event};
    return os_ipc_send((uint64_t)display,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}
static int flush_motion(int64_t display,int32_t*dx,int32_t*dy,uint64_t*ticks){
    if(!*dx&&!*dy)return 0;
    obs_input_event_t event={OBS_INPUT_MOUSE_MOVE,0,*dx,(uint32_t)*dy,*ticks};
    *dx=*dy=0;
    return send_event(display,&event);
}

int obsidia_main(void){
    int64_t input=os_handle_find(OS_OBJECT_INPUT);if(input<0)return 1;
    int64_t display=-1;while(display<0){display=os_service_connect("display");if(display<0)sys_sleep(2);}
    static const char ready[]="inputd: normalized input burst forwarding ready\n";sys_fd_write(1,ready,sizeof(ready)-1);
    for(;;){
        obs_input_event_t event;
        if(os_input_read((uint64_t)input,(os_input_event_t*)&event)<=0)continue;
        int32_t dx=0,dy=0;uint64_t motion_ticks=event.ticks;
        for(uint32_t drained=0;;drained++){
            int is_motion=event.type==OBS_INPUT_MOUSE_MOVE||event.type==OBS_INPUT_RELATIVE;
            if(is_motion){
                int32_t add_x=0,add_y=0;
                if(event.type==OBS_INPUT_MOUSE_MOVE){add_x=event.value;add_y=signed_reserved(event.reserved);}
                else if(event.code==OBS_INPUT_AXIS_X)add_x=event.value;
                else if(event.code==OBS_INPUT_AXIS_Y)add_y=event.value;
                int64_t next_x=(int64_t)dx+add_x,next_y=(int64_t)dy+add_y;
                if(next_x>32767||next_x<-32768||next_y>32767||next_y<-32768){if(flush_motion(display,&dx,&dy,&motion_ticks)<0)return 2;next_x=add_x;next_y=add_y;}
                dx=(int32_t)next_x;dy=(int32_t)next_y;motion_ticks=event.ticks;
            }else{
                if(flush_motion(display,&dx,&dy,&motion_ticks)<0)return 2;
                if(send_event(display,&event)<0)return 2;
            }
            if(drained>=63||os_input_try_read((uint64_t)input,(os_input_event_t*)&event)<=0)break;
        }
        if(flush_motion(display,&dx,&dy,&motion_ticks)<0)return 2;
    }
}
