#include "syscall.h"
#include <obsidia/service.h>
#include <obsidia/settings.h>
#include <obsidia/theme.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/internal/settings_protocol.h>

#define MAX_SUBSCRIBERS 16
typedef struct{uint8_t used;int64_t endpoint;uint64_t owner_pid;uint32_t mask;}subscriber_t;
typedef struct{uint32_t id,category;obs_setting_value_t value;}setting_t;
static subscriber_t subscribers[MAX_SUBSCRIBERS];static uint32_t generation=1;
static setting_t values[]={
 {OBS_SETTING_APPEARANCE_THEME,OBS_SETTINGS_SUBSCRIBE_APPEARANCE,{OBS_SETTING_TYPE_ENUM,{.enumeration=OBS_THEME_DEFAULT}}},
 {OBS_SETTING_APPEARANCE_TEXT_SCALE,OBS_SETTINGS_SUBSCRIBE_APPEARANCE,{OBS_SETTING_TYPE_U32,{.u32=2}}},
 {OBS_SETTING_DESKTOP_BACKGROUND_MODE,OBS_SETTINGS_SUBSCRIBE_DESKTOP,{OBS_SETTING_TYPE_ENUM,{.enumeration=OBS_BACKGROUND_SUBTLE_BANDS}}},
 {OBS_SETTING_PANEL_PRIMARY_EDGE,OBS_SETTINGS_SUBSCRIBE_PANEL,{OBS_SETTING_TYPE_ENUM,{.enumeration=OBS_PANEL_TOP}}},
 {OBS_SETTING_PANEL_PRIMARY_SIZE,OBS_SETTINGS_SUBSCRIBE_PANEL,{OBS_SETTING_TYPE_U32,{.u32=46}}}
};
/* Storage-provider seam. V1 deliberately remains session-local until VFS
 * replacement/atomic-write semantics are mature enough for safe persistence. */
static int storage_load(void){return-1;}
static int storage_commit(uint32_t setting_id,const obs_setting_value_t*value){(void)setting_id;(void)value;return-1;}
static void write(const char*s){uint32_t n=0;while(s[n])n++;sys_fd_write(1,s,n);}
static int find(uint32_t id){for(uint32_t i=0;i<sizeof(values)/sizeof(values[0]);i++)if(values[i].id==id)return(int)i;return-1;}
static int same_value(const obs_setting_value_t*a,const obs_setting_value_t*b){if(a->type!=b->type)return 0;for(uint32_t i=0;i<sizeof(*a);i++)if(((const uint8_t*)a)[i]!=((const uint8_t*)b)[i])return 0;return 1;}
static int validate(int index,const obs_setting_value_t*v){
    if(index<0)return OBS_SETTINGS_UNKNOWN_ID;
    if(v->type!=values[index].value.type)return OBS_SETTINGS_WRONG_TYPE;
    if(values[index].id==OBS_SETTING_APPEARANCE_THEME)return v->enumeration<=OBS_THEME_ALTERNATE?0:OBS_SETTINGS_INVALID_VALUE;
    if(values[index].id==OBS_SETTING_APPEARANCE_TEXT_SCALE)return v->u32>=1&&v->u32<=3?0:OBS_SETTINGS_INVALID_VALUE;
    if(values[index].id==OBS_SETTING_DESKTOP_BACKGROUND_MODE)return v->enumeration>=OBS_BACKGROUND_SOLID&&v->enumeration<=OBS_BACKGROUND_SUBTLE_BANDS?0:OBS_SETTINGS_INVALID_VALUE;
    if(values[index].id==OBS_SETTING_PANEL_PRIMARY_EDGE)return v->enumeration==OBS_PANEL_TOP||v->enumeration==OBS_PANEL_BOTTOM?0:OBS_SETTINGS_INVALID_VALUE;
    if(values[index].id==OBS_SETTING_PANEL_PRIMARY_SIZE)return v->u32>=32&&v->u32<=160?0:OBS_SETTINGS_INVALID_VALUE;
    return OBS_SETTINGS_INVALID_VALUE;
}
static void close_sub(int i){if(subscribers[i].used)os_handle_close((uint64_t)subscribers[i].endpoint);subscribers[i]=(subscriber_t){0};}
static void sweep(void){for(int i=0;i<MAX_SUBSCRIBERS;i++)if(subscribers[i].used&&(os_process_alive(subscribers[i].owner_pid)<=0||os_handle_has_remote((uint64_t)subscribers[i].endpoint)<=0)){close_sub(i);write("settings: dead subscriber reclaimed\n");}}
static void respond(int64_t reply,int status,uint32_t id,const obs_setting_value_t*value){if(reply<0)return;obs_settings_response_t r={status,OBS_SETTINGS_PROTOCOL_VERSION,id,generation,{0}};if(value)r.value=*value;os_ipc_try_send((uint64_t)reply,&r,sizeof(r));os_handle_close((uint64_t)reply);}
static void notify(const setting_t*s){obs_settings_response_t r={0,OBS_SETTINGS_PROTOCOL_VERSION,s->id,generation,s->value};for(int i=0;i<MAX_SUBSCRIBERS;i++)if(subscribers[i].used&&(subscribers[i].mask&s->category))os_ipc_try_send((uint64_t)subscribers[i].endpoint,&r,sizeof(r));}
static void subscribe(const obs_settings_request_t*q,const os_ipc_message_info_t*info){int slot=-1;if(!q->subscription_mask||(q->subscription_mask&~OBS_SETTINGS_SUBSCRIBE_ALL)){respond(info->attached,OBS_SETTINGS_INVALID_VALUE,0,0);return;}for(int i=0;i<MAX_SUBSCRIBERS;i++)if(!subscribers[i].used){slot=i;break;}if(slot<0){respond(info->attached,OBS_SETTINGS_ERROR,0,0);return;}subscribers[slot]=(subscriber_t){1,info->attached,info->sender_pid,q->subscription_mask};obs_settings_response_t r={0,OBS_SETTINGS_PROTOCOL_VERSION,0,generation,{0}};if(os_ipc_try_send((uint64_t)info->attached,&r,sizeof(r))!=(int64_t)sizeof(r))close_sub(slot);}
static void dispatch(const uint8_t*data,int64_t size,const os_ipc_message_info_t*info,int privileged){
    if(size!=(int64_t)sizeof(obs_settings_request_t)){respond(info->attached,OBS_SETTINGS_ERROR,0,0);return;}const obs_settings_request_t*q=(const obs_settings_request_t*)data;
    if(q->version!=OBS_SETTINGS_PROTOCOL_VERSION){respond(info->attached,OBS_SETTINGS_BAD_VERSION,q->setting_id,0);return;}
    if(q->operation==OBS_SETTINGS_OP_SUBSCRIBE&&!privileged){subscribe(q,info);return;}
    if(q->operation==OBS_SETTINGS_OP_QUERY_SUBSCRIBERS&&!privileged){obs_setting_value_t v={OBS_SETTING_TYPE_U32,{.u32=0}};for(int i=0;i<MAX_SUBSCRIBERS;i++)if(subscribers[i].used)v.u32++;respond(info->attached,0,0,&v);return;}
    int index=find(q->setting_id);if(q->operation==OBS_SETTINGS_OP_GET&&!privileged){if(index<0)respond(info->attached,OBS_SETTINGS_UNKNOWN_ID,q->setting_id,0);else respond(info->attached,0,q->setting_id,&values[index].value);return;}
    if(q->operation==OBS_SETTINGS_OP_SET&&privileged){int status=validate(index,&q->value);if(status<0){respond(info->attached,status,q->setting_id,0);return;}if(!same_value(&values[index].value,&q->value)){values[index].value=q->value;(void)storage_commit(q->setting_id,&q->value);generation++;if(!generation)generation=1;notify(&values[index]);if(q->setting_id==OBS_SETTING_APPEARANCE_THEME)write(q->value.enumeration==OBS_THEME_ALTERNATE?"settings: theme -> alternate\n":"settings: theme -> default\n");else if(q->setting_id==OBS_SETTING_PANEL_PRIMARY_EDGE)write(q->value.enumeration==OBS_PANEL_BOTTOM?"settings: panel edge -> bottom\n":"settings: panel edge -> top\n");else if(q->setting_id==OBS_SETTING_PANEL_PRIMARY_SIZE)write("settings: panel size changed\n");else if(q->setting_id==OBS_SETTING_APPEARANCE_TEXT_SCALE)write("settings: text scale changed\n");}respond(info->attached,0,q->setting_id,&values[index].value);return;}
    respond(info->attached,privileged?OBS_SETTINGS_ERROR:OBS_SETTINGS_DENIED,q->setting_id,0);
}
static int poll(int64_t endpoint,int privileged){uint8_t message[64];os_ipc_message_info_t info={-1,0};int64_t n=os_ipc_recv_ex((uint64_t)endpoint,message,sizeof(message),&info,1);if(n==-2)return 0;if(n<0)return 0;dispatch(message,n,&info,privileged);return 1;}
int obsidia_main(void){int64_t authority=os_handle_find(OS_OBJECT_IPC);if(authority<0)return 1;os_handle_set_inheritable((uint64_t)authority,0);(void)storage_load();int64_t endpoint=os_ipc_create();if(endpoint<0||os_service_register_restricted(OBS_SETTINGS_SERVICE_NAME,(uint64_t)endpoint,OS_RIGHT_WRITE|OS_RIGHT_DUP)<0)return 2;write("settings: service ready (in-memory provider)\n");for(;;){int worked=0;for(uint32_t i=0;i<32;i++){int n=poll(endpoint,0);worked|=n;if(!n)break;}for(uint32_t i=0;i<16;i++){int n=poll(authority,1);worked|=n;if(!n)break;}sweep();if(worked)sys_yield();else sys_sleep(1);}}
