#include "syscall.h"
#include <obsidia/app.h>
#include <obsidia/input.h>
#include <obsidia/theme.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/window.h>
#include <obsidia/service.h>
#include <obsidia/internal/display_protocol.h>
#include <obsidia/internal/desktop.h>

static int64_t display=-1;

static int stats(obs_display_stats_response_t*out){
    int64_t reply=os_ipc_create();if(reply<0)return-1;
    obs_display_query_request_t request={OBS_DISPLAY_QUERY_STATS,0};
    if(os_ipc_send_handle((uint64_t)display,&request,sizeof(request),(uint64_t)reply,OS_RIGHT_WRITE|OS_RIGHT_DUP)!=(int64_t)sizeof(request)){os_handle_close((uint64_t)reply);return-1;}
    int64_t received=os_ipc_recv((uint64_t)reply,out,sizeof(*out));os_handle_close((uint64_t)reply);
    return received==(int64_t)sizeof(*out)&&out->status==0?0:-1;
}

static int raw_window(uint32_t operation,uint32_t id){
    obs_display_window_request_t request={operation,id};
    return os_ipc_send((uint64_t)display,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}

static int raw_resize(uint32_t id,uint32_t width,uint32_t height){obs_display_resize_request_t request={OBS_DISPLAY_RESIZE_WINDOW,id,width,height};return os_ipc_send((uint64_t)display,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;}
static int wait_resize(obs_window_t*w,uint32_t width,uint32_t height){for(uint32_t i=0;i<16;i++){obs_window_event_t event;if(os_window_next_event(w,&event)<0)return-1;if(event.type==OBS_WINDOW_EVENT_RESIZE)return event.width==width&&event.height==height?0:-1;}return-1;}
static int abort_resize(obs_window_t*w,uint32_t width,uint32_t height){
    if(raw_resize(w->id,width,height)<0)return-1;
    for(uint32_t i=0;i<16;i++){obs_window_event_t event;int64_t attached=-1,n=os_ipc_recv_handle(w->session_handle,&event,sizeof(event),&attached);if(n==-3){if(os_ipc_recv(w->session_handle,&event,sizeof(event))!=(int64_t)sizeof(event))return-1;continue;}if(n!=(int64_t)sizeof(event)||attached<0||event.type!=OBS_WINDOW_EVENT_RESIZE)return-1;obs_display_resize_reply_t reply={OBS_DISPLAY_RESIZE_REPLY,w->id,event.code,OBS_DISPLAY_RESIZE_ABORT};os_handle_close((uint64_t)attached);return os_ipc_send((uint64_t)display,&reply,sizeof(reply))==(int64_t)sizeof(reply)?0:-1;}return-1;
}

static int inject(uint32_t type,uint32_t code,int32_t value){
    obs_display_input_request_t request={OBS_DISPLAY_FORWARD_INPUT,{type,code,value,0,sys_getticks()}};
    return os_ipc_send((uint64_t)display,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}
static int inject_move(int32_t dx,int32_t dy){
    obs_display_input_request_t request={OBS_DISPLAY_FORWARD_INPUT,{OBS_INPUT_MOUSE_MOVE,0,dx,(uint32_t)dy,sys_getticks()}};
    return os_ipc_send((uint64_t)display,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}

int obsidia_main(void){
    display=os_service_connect("display");if(display<0)return 1;
    obs_display_stats_response_t before,after;if(stats(&before)<0||before.live_windows)return 2;

    /* A rapid burst must be accumulated exactly once.  Returning to the
     * baseline also leaves the coordinate assumptions in later drag tests
     * unchanged. */
    int32_t motion_x=before.cursor_x,motion_y=before.cursor_y;
    uint64_t motion_events=before.input_events;
    for(uint32_t i=0;i<100;i++)if(inject_move(1,0)<0)return 101;
    if(stats(&after)<0||after.cursor_x!=motion_x+100||after.cursor_y!=motion_y||after.input_events!=motion_events+100)return 102;
    if(inject_move(-100,0)<0||stats(&after)<0||after.cursor_x!=motion_x||after.cursor_y!=motion_y)return 103;
    sys_sleep(3);if(stats(&after)<0||after.cursor_x!=motion_x||after.cursor_y!=motion_y)return 104;

    for(uint32_t cycle=0;cycle<70;cycle++){
        obs_window_t window;if(os_window_create(&window,120,80,"Lifecycle stress")<0)return 3;
        window.pixels[0]=cycle;if(os_window_present(&window)<0)return 4;
        if(os_window_close(&window)<0)return 5;
        if(os_window_close(&window)>=0||os_window_present(&window)>=0)return 6;
    }
    if(stats(&after)<0||after.live_windows||after.damage_events<=before.damage_events)return 7;

    obs_window_t first,second;if(os_window_create(&first,120,80,"Stale first")<0)return 8;
    uint32_t stale_id=first.id;if(os_window_close(&first)<0)return 9;
    if(os_window_create(&second,120,80,"Stale second")<0||second.id==stale_id)return 10;
    if(stats(&before)<0||before.live_windows!=1)return 11;
    if(raw_window(OBS_DISPLAY_PRESENT_WINDOW,stale_id)<0||raw_window(OBS_DISPLAY_CLOSE_WINDOW,stale_id)<0||raw_window(OBS_DISPLAY_CLOSE_WINDOW,0x12345678U)<0)return 12;
    if(stats(&after)<0||after.live_windows!=1||after.rejected_requests<before.rejected_requests+3)return 13;
    if(os_window_close(&second)<0||stats(&after)<0||after.live_windows)return 14;

    int64_t control=os_ipc_create();if(control<0||os_handle_set_inheritable((uint64_t)control,1)<0)return 15;
    int64_t child=obs_app_launch("window-abrupt" OBS_NATIVE_EXEC_EXTENSION);if(child<0)return 16;
    uint32_t foreign_id=0;if(os_ipc_recv((uint64_t)control,&foreign_id,sizeof(foreign_id))!=(int64_t)sizeof(foreign_id))return 17;
    os_handle_set_inheritable((uint64_t)control,0);
    if(stats(&before)<0||before.live_windows!=1)return 18;
    if(raw_window(OBS_DISPLAY_PRESENT_WINDOW,foreign_id)<0||raw_window(OBS_DISPLAY_CLOSE_WINDOW,foreign_id)<0)return 19;
    if(stats(&after)<0||after.live_windows!=1||after.rejected_requests<before.rejected_requests+2)return 20;

    if(inject(OBS_INPUT_RELATIVE,OBS_INPUT_AXIS_X,-340)<0||inject(OBS_INPUT_RELATIVE,OBS_INPUT_AXIS_Y,-248)<0||inject(OBS_INPUT_BUTTON,OBS_INPUT_BUTTON_LEFT,1)<0)return 21;
    int64_t ack=os_ipc_create();if(ack<0)return 22;
    uint8_t exit_command=3,detached_ack=0;
    if(os_ipc_send_handle((uint64_t)control,&exit_command,1,(uint64_t)ack,OS_RIGHT_WRITE)!=(int64_t)1)return 22;
    if(os_ipc_recv((uint64_t)ack,&detached_ack,1)!=1||detached_ack!=1)return 23;
    os_handle_close((uint64_t)ack);
    if(stats(&after)<0||after.live_windows)return 23;
    if(inject(OBS_INPUT_RELATIVE,OBS_INPUT_AXIS_X,20)<0||inject(OBS_INPUT_BUTTON,OBS_INPUT_BUTTON_LEFT,0)<0)return 24;
    int64_t status=-1;if(sys_wait((uint64_t)child,&status)<0||status)return 25;

    for(uint8_t mode=1;mode<=2;mode++){
        os_handle_close((uint64_t)control);control=os_ipc_create();if(control<0||os_handle_set_inheritable((uint64_t)control,1)<0)return 26;
        child=obs_app_launch("window-abrupt" OBS_NATIVE_EXEC_EXTENSION);if(child<0)return 27;
        if(os_ipc_recv((uint64_t)control,&foreign_id,sizeof(foreign_id))!=(int64_t)sizeof(foreign_id))return 28;
        os_handle_set_inheritable((uint64_t)control,0);if(os_ipc_send((uint64_t)control,&mode,1)!=1)return 29;
        status=0;if(sys_wait((uint64_t)child,&status)<0||(mode==1?status!=0:status>=0))return 30;
        sys_sleep(3);if(stats(&after)<0||after.live_windows)return 31;
    }

    obs_window_t resized;if(os_window_create(&resized,180,100,"Resize stress")<0)return 32;
    if(stats(&before)<0)return 33;
    for(uint32_t cycle=0;cycle<32;cycle++){uint32_t width=(cycle&1)?180:360,height=(cycle&1)?100:210;if(os_window_resize(&resized,width,height)<0||wait_resize(&resized,width,height)<0){return 34;}for(uint32_t p=0;p<resized.width*resized.height;p+=257)resized.pixels[p]=cycle;if(os_window_present(&resized)<0)return 35;}
    const obs_theme_t*theme=obs_theme_get_default();uint32_t maximized_width=1280-theme->metrics.border_width*2,maximized_height=720-theme->metrics.titlebar_height-theme->metrics.border_width;
    for(uint32_t cycle=0;cycle<4;cycle++){if(os_window_maximize(&resized)<0||wait_resize(&resized,maximized_width,maximized_height)<0)return 36;if(resized.state!=OBS_WINDOW_MAXIMIZED)return 37;if(os_window_restore(&resized)<0||wait_resize(&resized,180,100)<0)return 38;if(resized.state!=OBS_WINDOW_NORMAL)return 39;}
    if(abort_resize(&resized,260,150)<0)return 40;
    sys_sleep(2);if(stats(&after)<0||after.resize_commits<before.resize_commits+40||after.resize_aborts<=before.resize_aborts)return 41;
    if(raw_resize(resized.id,0,100)<0||raw_resize(resized.id,120,0)<0)return 42;
    sys_sleep(2);if(stats(&after)<0||after.rejected_requests<before.rejected_requests+2)return 43;
    if(os_window_minimize(&resized)<0||os_window_restore(&resized)<0)return 44;
    uint32_t resize_stale=resized.id;if(os_window_close(&resized)<0)return 45;
    if(os_window_create(&resized,180,100,"Resize reuse")<0||resized.id==resize_stale)return 46;
    if(raw_resize(resize_stale,240,140)<0)return 47;
    sys_sleep(2);if(stats(&after)<0||after.live_windows!=1)return 48;
    if(raw_resize(resized.id,240,140)<0||os_window_close(&resized)<0)return 49;
    sys_sleep(2);if(stats(&after)<0||after.live_windows)return 50;

    for(uint8_t mode=4;mode<=6;mode++){
        os_handle_close((uint64_t)control);control=os_ipc_create();if(control<0||os_handle_set_inheritable((uint64_t)control,1)<0)return 51;child=obs_app_launch("window-abrupt" OBS_NATIVE_EXEC_EXTENSION);if(child<0)return 52;if(os_ipc_recv((uint64_t)control,&foreign_id,sizeof(foreign_id))!=(int64_t)sizeof(foreign_id))return 53;os_handle_set_inheritable((uint64_t)control,0);
        if(mode==4){if(stats(&before)<0||raw_resize(foreign_id,240,140)<0)return 54;sys_sleep(2);if(stats(&after)<0||after.rejected_requests<=before.rejected_requests)return 54;}
        if(os_ipc_send((uint64_t)control,&mode,1)!=1)return 55;
        status=0;if(sys_wait((uint64_t)child,&status)<0||(mode==5?status>=0:status!=0))return 56;sys_sleep(3);if(stats(&after)<0||after.live_windows)return 57;
    }

    fb_info_t fb;sys_fbinfo(&fb);obs_work_area_t work;if(obs_desktop_work_area(obs_desktop_layout_default(),(uint32_t)fb.width,(uint32_t)fb.height,&work)<0)return 58;obs_window_t root;if(obs_desktop_create_root(&root,(uint32_t)fb.width,(uint32_t)fb.height)<0||obs_desktop_configure_work_area(&root,work.x,work.y,work.width,work.height)<0)return 58;obs_shell_overlay_t overlay;if(obs_desktop_overlay_create(&root,&overlay,240,160)<0||obs_desktop_overlay_configure(&root,&overlay,12,work.y+8,1)<0||obs_desktop_overlay_configure(&root,&overlay,12,(int32_t)fb.height-work.y-168,1)<0)return 72;
    obs_window_t managed;if(os_window_create(&managed,180,100,"Managed")<0)return 59;uint32_t managed_id=managed.id;int saw_added=0,saw_removed=0;
    uint32_t managed_max_width=(uint32_t)fb.width-theme->metrics.border_width*2,managed_max_height=(uint32_t)fb.height-work.y-theme->metrics.titlebar_height-theme->metrics.border_width;
    if(os_window_maximize(&managed)<0||wait_resize(&managed,managed_max_width,managed_max_height)<0)return 68;
    if(obs_desktop_configure_work_area(&root,0,0,(uint32_t)fb.width,(uint32_t)fb.height-work.y)<0||wait_resize(&managed,managed_max_width,managed_max_height)<0)return 69;
    if(os_window_restore(&managed)<0||wait_resize(&managed,180,100)<0||managed.state!=OBS_WINDOW_NORMAL)return 70;
    if(obs_desktop_configure_work_area(&root,work.x,work.y,work.width,work.height)<0)return 71;
    for(uint32_t i=0;i<8&&!saw_added;i++){obs_window_event_t event;obs_desktop_management_event_t m;if(obs_desktop_next_event(&root,&event,&m)<0)return 60;if(m.type==OBS_DESKTOP_EVENT_WINDOW_ADDED&&m.window_id==managed_id&&m.owner_pid==sys_getpid()&&m.title[0]=='M'&&m.title[6]=='d'&&!m.title[7])saw_added=1;}if(!saw_added)return 61;
    if(os_window_minimize(&managed)<0||obs_desktop_focus_restore(&root,managed_id)<0||os_window_close(&managed)<0)return 62;
    for(uint32_t i=0;i<8&&!saw_removed;i++){obs_window_event_t event;obs_desktop_management_event_t m;if(obs_desktop_next_event(&root,&event,&m)<0)return 63;if(m.type==OBS_DESKTOP_EVENT_WINDOW_REMOVED&&m.window_id==managed_id)saw_removed=1;}if(!saw_removed)return 64;
    if(stats(&before)<0||obs_desktop_focus_restore(&root,managed_id)<0)return 65;
    sys_sleep(2);if(stats(&after)<0||after.rejected_requests<=before.rejected_requests)return 66;if(os_window_close(&root)<0)return 67;obs_desktop_overlay_close(&overlay);if(obs_desktop_create_root(&root,(uint32_t)fb.width,(uint32_t)fb.height)<0||obs_desktop_overlay_create(&root,&overlay,240,160)<0)return 73;obs_desktop_overlay_close(&overlay);if(os_window_close(&root)<0)return 74;
    os_handle_close((uint64_t)control);os_handle_close((uint64_t)display);
    static const char passed[]="window-lifecycle: motion, resize/lifecycle, owner PID and shell-overlay cleanup passed\n";
    sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
