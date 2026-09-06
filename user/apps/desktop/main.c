#include "syscall.h"
#include <obsidia/service.h>
#define DESKTOP_MAP 0x0000006000000000ULL
static void rectangle(uint32_t*p,uint32_t width,uint32_t height,uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t color){
    if(x>=width||y>=height)return;
    if(w>width-x)w=width-x;
    if(h>height-y)h=height-y;
    for(uint32_t row=0;row<h;row++)for(uint32_t column=0;column<w;column++)p[(y+row)*width+x+column]=color;
}
static void paint(uint32_t*p,uint32_t width,uint32_t height,uint32_t accent){
    rectangle(p,width,height,0,0,width,height,0x00101522);rectangle(p,width,height,0,0,width,42,0x001b2433);
    rectangle(p,width,height,0,41,width,1,0x00313f55);rectangle(p,width,height,34,76,112,112,0x001b2638);
    rectangle(p,width,height,42,84,96,96,0x00253348);rectangle(p,width,height,65,103,50,50,accent);
    rectangle(p,width,height,75,113,30,30,0x00101522);rectangle(p,width,height,34,196,112,4,accent);
    rectangle(p,width,height,width>170?width-146:0,12,112,18,0x002a374a);
}
int obsidia_main(void){
    int64_t display=os_service_connect("display");if(display<0)return 1;
    static const char hello[]="desktop-online";os_ipc_send((uint64_t)display,hello,sizeof(hello)-1);
    fb_info_t info;sys_fbinfo(&info);if(!info.width||!info.height||info.width>4096||info.height>4096)return 2;
    int64_t surface=os_surface_create((uint32_t)info.width,(uint32_t)info.height);if(surface<0)return 3;
    uint32_t*pixels=os_shm_map((uint64_t)surface,(void*)DESKTOP_MAP,1);if(pixels==(void*)-1)return 4;
    uint32_t accent=0x005b8cff;paint(pixels,(uint32_t)info.width,(uint32_t)info.height,accent);
    if(os_surface_present((uint64_t)surface,0,0)<0)return 5;
    static const char ready[]="desktop: surface presented; input loop ready\n";sys_fd_write(1,ready,sizeof(ready)-1);
    int64_t input=os_handle_find(OS_OBJECT_INPUT);if(input<0)return 6;
    for(;;){os_input_event_t event;if(os_input_read((uint64_t)input,&event)>0&&event.value){
        accent=accent==0x005b8cff?0x00c55cff:0x005b8cff;paint(pixels,(uint32_t)info.width,(uint32_t)info.height,accent);os_surface_present((uint64_t)surface,0,0);
    }}
}
