#include <obsidia/settings.h>
#include <obsidia/service.h>
#include <obsidia/internal/settings_protocol.h>
#include "syscall.h"

static void clear(obs_settings_t*s){
    if(!s)return;
    s->service_handle=s->event_handle=s->authority_handle=(uint64_t)-1;
    s->subscription_mask=0;
}
static int connect_common(obs_settings_t*s,uint64_t authority){
    if(!s)return OBS_SETTINGS_ERROR;
    clear(s);
    int64_t service=os_service_connect(OBS_SETTINGS_SERVICE_NAME);
    if(service<0)return OBS_SETTINGS_ERROR;
    s->service_handle=(uint64_t)service;
    s->authority_handle=authority;
    return OBS_SETTINGS_OK;
}
int obs_settings_connect(obs_settings_t*s){return connect_common(s,(uint64_t)-1);}
int obs_settings_connect_with_authority(obs_settings_t*s,uint64_t authority){if(authority==(uint64_t)-1)return OBS_SETTINGS_ERROR;return connect_common(s,authority);}
static int request_reply(uint64_t endpoint,const obs_settings_request_t*request,obs_settings_response_t*response){
    int64_t reply=os_ipc_create();if(reply<0)return OBS_SETTINGS_ERROR;
    int result=OBS_SETTINGS_ERROR;if(os_ipc_send_handle(endpoint,request,sizeof(*request),(uint64_t)reply,OS_RIGHT_WRITE)==(int64_t)sizeof(*request)&&os_ipc_recv((uint64_t)reply,response,sizeof(*response))==(int64_t)sizeof(*response))result=response->status;
    os_handle_close((uint64_t)reply);return result;
}
int obs_settings_get(obs_settings_t*s,uint32_t id,obs_setting_value_t*value){
    if(!s||!value||s->service_handle==(uint64_t)-1)return OBS_SETTINGS_ERROR;
    obs_settings_request_t q={OBS_SETTINGS_PROTOCOL_VERSION,OBS_SETTINGS_OP_GET,id,0,{0}};
    obs_settings_response_t r={0};
    int status=request_reply(s->service_handle,&q,&r);
    if(status==0)*value=r.value;
    return status;
}
int obs_settings_set(obs_settings_t*s,uint32_t id,const obs_setting_value_t*value){
    if(!s||!value||s->authority_handle==(uint64_t)-1)return OBS_SETTINGS_DENIED;
    obs_settings_request_t q={OBS_SETTINGS_PROTOCOL_VERSION,OBS_SETTINGS_OP_SET,id,0,*value};
    obs_settings_response_t r={0};
    return request_reply(s->authority_handle,&q,&r);
}
int obs_settings_subscribe(obs_settings_t*s,uint32_t mask){
    if(!s||s->service_handle==(uint64_t)-1||!mask||(mask&~OBS_SETTINGS_SUBSCRIBE_ALL))return OBS_SETTINGS_INVALID_VALUE;
    if(s->event_handle!=(uint64_t)-1)os_handle_close(s->event_handle);
    int64_t events=os_ipc_create();
    if(events<0)return OBS_SETTINGS_ERROR;
    obs_settings_request_t q={OBS_SETTINGS_PROTOCOL_VERSION,OBS_SETTINGS_OP_SUBSCRIBE,0,mask,{0}};obs_settings_response_t r={0};
    if(os_ipc_send_handle(s->service_handle,&q,sizeof(q),(uint64_t)events,OS_RIGHT_WRITE|OS_RIGHT_DUP)!=(int64_t)sizeof(q)||os_ipc_recv((uint64_t)events,&r,sizeof(r))!=(int64_t)sizeof(r)||r.status<0){os_handle_close((uint64_t)events);return r.status<0?r.status:OBS_SETTINGS_ERROR;}
    s->event_handle=(uint64_t)events;s->subscription_mask=mask;return OBS_SETTINGS_OK;
}
int obs_settings_try_next(obs_settings_t*s,obs_settings_event_t*event){
    if(!s||!event||s->event_handle==(uint64_t)-1)return OBS_SETTINGS_ERROR;
    obs_settings_response_t r={0};
    os_ipc_message_info_t info={-1,0};
    int64_t n=os_ipc_recv_ex(s->event_handle,&r,sizeof(r),&info,1);
    if(info.attached>=0)os_handle_close((uint64_t)info.attached);
    if(n==-2)return 0;
    if(n!=(int64_t)sizeof(r)||r.status<0||r.version!=OBS_SETTINGS_PROTOCOL_VERSION)return OBS_SETTINGS_ERROR;
    event->setting_id=r.setting_id;
    event->generation=r.generation;
    event->value=r.value;
    return 1;
}
int obs_settings_query_subscribers(obs_settings_t*s,uint32_t*count){if(!s||!count||s->service_handle==(uint64_t)-1)return OBS_SETTINGS_ERROR;obs_settings_request_t q={OBS_SETTINGS_PROTOCOL_VERSION,OBS_SETTINGS_OP_QUERY_SUBSCRIBERS,0,0,{0}};obs_settings_response_t r={0};int status=request_reply(s->service_handle,&q,&r);if(!status)*count=r.value.u32;return status;}
static int pin_request(uint64_t endpoint,uint32_t operation,uint32_t index,const char*id,obs_settings_pin_response_t*r){
    obs_settings_pin_request_t q={OBS_SETTINGS_PROTOCOL_VERSION,operation,index,0,{0}};uint32_t i=0;if(id){for(;i<sizeof(q.application_id)-1&&id[i];i++)q.application_id[i]=id[i];if(id[i])return OBS_SETTINGS_INVALID_VALUE;}
    int64_t reply=os_ipc_create();if(reply<0)return OBS_SETTINGS_ERROR;int result=OBS_SETTINGS_ERROR;
    if(os_ipc_send_handle(endpoint,&q,sizeof(q),(uint64_t)reply,OS_RIGHT_WRITE)==(int64_t)sizeof(q)&&os_ipc_recv((uint64_t)reply,r,sizeof(*r))==(int64_t)sizeof(*r))result=r->status;
    os_handle_close((uint64_t)reply);return result;
}
int obs_settings_get_pinned_apps(obs_settings_t*s,obs_pinned_apps_t*pins){
    if(!s||!pins||s->service_handle==(uint64_t)-1)return OBS_SETTINGS_ERROR;
    pins->count=0;
    obs_settings_pin_response_t r={0};
    int status=pin_request(s->service_handle,OBS_SETTINGS_OP_GET_PIN,0,0,&r);
    if(status<0)return status;
    if(r.count>OBS_SETTINGS_MAX_PINNED_APPS)return OBS_SETTINGS_ERROR;
    for(uint32_t i=0;i<r.count;i++){if(i&&pin_request(s->service_handle,OBS_SETTINGS_OP_GET_PIN,i,0,&r)<0)return OBS_SETTINGS_ERROR;for(uint32_t n=0;n<OBS_APP_ID_MAX;n++)pins->ids[i][n]=r.application_id[n];pins->ids[i][OBS_APP_ID_MAX-1]=0;}pins->count=r.count;return 0;
}
int obs_settings_pin_app(obs_settings_t*s,const char*id){if(!s||s->authority_handle==(uint64_t)-1)return OBS_SETTINGS_DENIED;obs_settings_pin_response_t r={0};return pin_request(s->authority_handle,OBS_SETTINGS_OP_PIN,0,id,&r);}
int obs_settings_unpin_app(obs_settings_t*s,const char*id){if(!s||s->authority_handle==(uint64_t)-1)return OBS_SETTINGS_DENIED;obs_settings_pin_response_t r={0};return pin_request(s->authority_handle,OBS_SETTINGS_OP_UNPIN,0,id,&r);}
void obs_settings_close(obs_settings_t*s){if(!s)return;if(s->event_handle!=(uint64_t)-1)os_handle_close(s->event_handle);if(s->service_handle!=(uint64_t)-1)os_handle_close(s->service_handle);clear(s);}
