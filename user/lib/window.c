#include <obsidia/window.h>
#include <obsidia/service.h>
#include <obsidia/internal/desktop.h>
#include <obsidia/internal/display_protocol.h>
#include "syscall.h"

#define WINDOW_MAP_BASE 0x0000005000000000ULL
#define WINDOW_MAP_STRIDE 0x01000000ULL
#define WINDOW_MAP_ALTERNATE 0x00800000ULL
#define SHELL_OVERLAY_MAP 0x0000005180000000ULL
static uint32_t next_mapping_slot;

static int create_window(obs_window_t*window,uint32_t width,uint32_t height,const char*title,uint32_t operation){
    if(!window)return-1;
    for(uint32_t i=0;i<sizeof(*window);i++)((uint8_t*)window)[i]=0;
    window->display_handle=(uint64_t)-1;window->session_handle=(uint64_t)-1;window->surface_handle=(uint64_t)-1;
    int64_t display=os_service_connect("display"),session=os_ipc_create();if(display<0||session<0)goto fail;
    obs_display_create_request_t request={0};request.operation=operation;request.width=width;request.height=height;
    if(title){uint32_t i=0;for(;i<sizeof(request.title)-1&&title[i];i++)request.title[i]=title[i];}
    if(os_ipc_send_handle((uint64_t)display,&request,sizeof(request),(uint64_t)session,OS_RIGHT_WRITE|OS_RIGHT_DUP)!=(int64_t)sizeof(request))goto fail;
    obs_display_create_response_t response;int64_t surface=-1;int64_t received=os_ipc_recv_handle((uint64_t)session,&response,sizeof(response),&surface);
    if(received==-3)received=os_ipc_recv((uint64_t)session,&response,sizeof(response));
    if(received!=(int64_t)sizeof(response)||response.status<0||surface<0)goto fail;
    uint64_t mapping=WINDOW_MAP_BASE+(uint64_t)(next_mapping_slot++&255U)*WINDOW_MAP_STRIDE;
    uint32_t*pixels=os_shm_map((uint64_t)surface,(void*)mapping,1);if(pixels==(void*)-1){os_handle_close((uint64_t)surface);goto fail;}
    window->display_handle=(uint64_t)display;window->session_handle=(uint64_t)session;window->surface_handle=(uint64_t)surface;
    window->pixels=pixels;window->mapping_address=mapping;window->id=response.window_id;window->width=response.width;window->height=response.height;window->state=OBS_WINDOW_NORMAL;return 0;
fail:if(display>=0)os_handle_close((uint64_t)display);if(session>=0)os_handle_close((uint64_t)session);return-1;
}
int os_window_create(obs_window_t*w,uint32_t width,uint32_t height,const char*title){return create_window(w,width,height,title,OBS_DISPLAY_CREATE_WINDOW);}
int obs_desktop_create_root(obs_window_t*w,uint32_t width,uint32_t height){return create_window(w,width,height,"desktop",OBS_DISPLAY_CREATE_ROOT);}
uint32_t*os_window_pixels(obs_window_t*w){return w?w->pixels:0;}
int os_window_present(obs_window_t*w){if(!w||w->display_handle==(uint64_t)-1)return-1;obs_display_window_request_t request={OBS_DISPLAY_PRESENT_WINDOW,w->id};return os_ipc_send(w->display_handle,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;}
int os_window_next_event(obs_window_t*w,obs_window_event_t*event){
    if(!w||!event||w->session_handle==(uint64_t)-1)return-1;
    int64_t replacement=-1;int64_t received=os_ipc_recv_handle(w->session_handle,event,sizeof(*event),&replacement);
    if(received==-3)return os_ipc_recv(w->session_handle,event,sizeof(*event))==(int64_t)sizeof(*event)?1:-1;
    if(received!=(int64_t)sizeof(*event)||replacement<0){if(replacement>=0)os_handle_close((uint64_t)replacement);return-1;}
    if(event->type!=OBS_WINDOW_EVENT_RESIZE||!event->width||!event->height){os_handle_close((uint64_t)replacement);return-1;}
    uint64_t mapping=w->mapping_address^WINDOW_MAP_ALTERNATE;
    uint32_t*pixels=os_shm_map((uint64_t)replacement,(void*)mapping,1);
    obs_display_resize_reply_t reply={OBS_DISPLAY_RESIZE_REPLY,w->id,event->code,pixels==(void*)-1?OBS_DISPLAY_RESIZE_ABORT:OBS_DISPLAY_RESIZE_ACCEPT};
    if(os_ipc_send(w->display_handle,&reply,sizeof(reply))!=(int64_t)sizeof(reply)||pixels==(void*)-1){
        if(pixels!=(void*)-1)os_shm_unmap((void*)mapping);
        os_handle_close((uint64_t)replacement);return-1;
    }
    os_shm_unmap((void*)w->mapping_address);os_handle_close(w->surface_handle);
    w->surface_handle=(uint64_t)replacement;w->pixels=pixels;w->mapping_address=mapping;w->width=event->width;w->height=event->height;w->state=(uint32_t)event->value;
    return 1;
}
int os_window_try_next_event(obs_window_t*w,obs_window_event_t*event){
    if(!w||!event||w->session_handle==(uint64_t)-1)return-1;
    os_ipc_message_info_t info={-1,0};
    int64_t received=os_ipc_recv_ex(w->session_handle,event,sizeof(*event),&info,1);
    int64_t replacement=info.attached;
    if(received==-2)return 0;
    if(received!=(int64_t)sizeof(*event)){if(replacement>=0)os_handle_close((uint64_t)replacement);return-1;}if(replacement<0)return 1;
    if(event->type!=OBS_WINDOW_EVENT_RESIZE||!event->width||!event->height){os_handle_close((uint64_t)replacement);return-1;}uint64_t mapping=w->mapping_address^WINDOW_MAP_ALTERNATE;uint32_t*pixels=os_shm_map((uint64_t)replacement,(void*)mapping,1);obs_display_resize_reply_t reply={OBS_DISPLAY_RESIZE_REPLY,w->id,event->code,pixels==(void*)-1?OBS_DISPLAY_RESIZE_ABORT:OBS_DISPLAY_RESIZE_ACCEPT};if(os_ipc_send(w->display_handle,&reply,sizeof(reply))!=(int64_t)sizeof(reply)||pixels==(void*)-1){if(pixels!=(void*)-1)os_shm_unmap((void*)mapping);os_handle_close((uint64_t)replacement);return-1;}os_shm_unmap((void*)w->mapping_address);os_handle_close(w->surface_handle);w->surface_handle=(uint64_t)replacement;w->pixels=pixels;w->mapping_address=mapping;w->width=event->width;w->height=event->height;w->state=(uint32_t)event->value;return 1;
}

static int window_action(obs_window_t*w,uint32_t action){
    if(!w||w->display_handle==(uint64_t)-1)return-1;
    obs_display_action_request_t request={OBS_DISPLAY_WINDOW_ACTION,w->id,action,0};
    return os_ipc_send(w->display_handle,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}

int os_window_resize(obs_window_t*w,uint32_t width,uint32_t height){
    if(!w||w->display_handle==(uint64_t)-1)return-1;
    obs_display_resize_request_t request={OBS_DISPLAY_RESIZE_WINDOW,w->id,width,height};
    return os_ipc_send(w->display_handle,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}
int os_window_minimize(obs_window_t*w){return window_action(w,OBS_DISPLAY_ACTION_MINIMIZE);}
int os_window_maximize(obs_window_t*w){return window_action(w,OBS_DISPLAY_ACTION_MAXIMIZE);}
int os_window_restore(obs_window_t*w){return window_action(w,OBS_DISPLAY_ACTION_RESTORE);}

int obs_desktop_configure_work_area(obs_window_t*w,int32_t x,int32_t y,uint32_t width,uint32_t height){
    if(!w||w->display_handle==(uint64_t)-1)return-1;
    obs_display_root_config_request_t request={OBS_DISPLAY_CONFIGURE_ROOT,w->id,x,y,width,height};
    return os_ipc_send(w->display_handle,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}

int obs_desktop_focus_restore(obs_window_t*w,uint32_t id){
    if(!w||w->display_handle==(uint64_t)-1)return-1;
    obs_display_manage_request_t request={OBS_DISPLAY_MANAGE_WINDOW,w->id,id,OBS_DISPLAY_MANAGE_FOCUS_RESTORE};
    return os_ipc_send(w->display_handle,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}
int obs_desktop_overlay_create(obs_window_t*root,obs_shell_overlay_t*overlay,uint32_t width,uint32_t height){
    if(!root||!overlay||root->display_handle==(uint64_t)-1||!width||!height)return-1;
    for(uint32_t i=0;i<sizeof(*overlay);i++)((uint8_t*)overlay)[i]=0;
    overlay->surface_handle=(uint64_t)-1;
    int64_t reply=os_ipc_create();if(reply<0)return-1;
    obs_display_overlay_create_request_t request={OBS_DISPLAY_CREATE_SHELL_OVERLAY,root->id,width,height};
    if(os_ipc_send_handle(root->display_handle,&request,sizeof(request),(uint64_t)reply,OS_RIGHT_WRITE)!=(int64_t)sizeof(request)){os_handle_close((uint64_t)reply);return-1;}
    obs_display_create_response_t response={0};int64_t surface=-1;
    int64_t received=os_ipc_recv_handle((uint64_t)reply,&response,sizeof(response),&surface);os_handle_close((uint64_t)reply);
    if(received!=(int64_t)sizeof(response)||response.status<0||surface<0)return-1;
    uint32_t*pixels=os_shm_map((uint64_t)surface,(void*)SHELL_OVERLAY_MAP,1);
    if(pixels==(void*)-1){os_handle_close((uint64_t)surface);return-1;}
    overlay->surface_handle=(uint64_t)surface;overlay->mapping_address=SHELL_OVERLAY_MAP;overlay->pixels=pixels;overlay->width=response.width;overlay->height=response.height;
    return 0;
}
int obs_desktop_overlay_configure(obs_window_t*root,obs_shell_overlay_t*overlay,int32_t x,int32_t y,int visible){
    if(!root||!overlay||overlay->surface_handle==(uint64_t)-1)return-1;
    obs_display_overlay_config_request_t request={OBS_DISPLAY_CONFIGURE_SHELL_OVERLAY,root->id,x,y,visible?1U:0U};
    return os_ipc_send(root->display_handle,&request,sizeof(request))==(int64_t)sizeof(request)?0:-1;
}
void obs_desktop_overlay_close(obs_shell_overlay_t*overlay){
    if(!overlay)return;
    if(overlay->pixels)os_shm_unmap((void*)overlay->mapping_address);
    if(overlay->surface_handle!=(uint64_t)-1)os_handle_close(overlay->surface_handle);
    for(uint32_t i=0;i<sizeof(*overlay);i++)((uint8_t*)overlay)[i]=0;
    overlay->surface_handle=(uint64_t)-1;
}
int obs_desktop_next_event(obs_window_t*w,obs_window_event_t*application,obs_desktop_management_event_t*management){
    if(!w||!application||!management||w->session_handle==(uint64_t)-1)return-1;
    union {obs_window_event_t application;obs_desktop_management_event_t management;} message={0};
    int64_t received=os_ipc_recv(w->session_handle,&message,sizeof(message));
    if(received==(int64_t)sizeof(message.application)){*application=message.application;return OBS_DESKTOP_NEXT_APPLICATION;}
    if(received==(int64_t)sizeof(message.management)){*management=message.management;return OBS_DESKTOP_NEXT_MANAGEMENT;}
    return-1;
}
int obs_desktop_try_next_event(obs_window_t*w,obs_window_event_t*application,obs_desktop_management_event_t*management){
    if(!w||!application||!management||w->session_handle==(uint64_t)-1)return-1;
    union {obs_window_event_t application;obs_desktop_management_event_t management;} message={0};
    os_ipc_message_info_t info={-1,0};
    int64_t received=os_ipc_recv_ex(w->session_handle,&message,sizeof(message),&info,1);
    if(info.attached>=0)os_handle_close((uint64_t)info.attached);
    if(received==-2)return 0;
    if(received==(int64_t)sizeof(message.application)){*application=message.application;return OBS_DESKTOP_NEXT_APPLICATION;}
    if(received==(int64_t)sizeof(message.management)){*management=message.management;return OBS_DESKTOP_NEXT_MANAGEMENT;}
    return-1;
}
int os_window_close(obs_window_t*w){
    if(!w||w->display_handle==(uint64_t)-1)return-1;
    obs_display_window_request_t request={OBS_DISPLAY_CLOSE_WINDOW,w->id};
    int result=-1;int64_t reply=os_ipc_create();
    if(reply>=0&&os_ipc_send_handle(w->display_handle,&request,sizeof(request),(uint64_t)reply,OS_RIGHT_WRITE)==(int64_t)sizeof(request)){
        obs_display_close_response_t response;
        if(os_ipc_recv((uint64_t)reply,&response,sizeof(response))==(int64_t)sizeof(response))result=response.status;
    }
    if(reply>=0)os_handle_close((uint64_t)reply);
    if(w->pixels)os_shm_unmap((void*)w->mapping_address);
    if(w->surface_handle!=(uint64_t)-1)os_handle_close(w->surface_handle);
    if(w->session_handle!=(uint64_t)-1)os_handle_close(w->session_handle);
    os_handle_close(w->display_handle);
    for(uint32_t i=0;i<sizeof(*w);i++)((uint8_t*)w)[i]=0;
    w->display_handle=w->session_handle=w->surface_handle=(uint64_t)-1;
    return result;
}
