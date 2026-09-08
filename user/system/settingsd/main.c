#include "syscall.h"
#include <obsidia/service.h>
#include <obsidia/settings.h>
#include <obsidia/theme.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/app_catalog.h>
#include <obsidia/internal/settings_protocol.h>

#define MAX_SUBSCRIBERS 16
typedef struct{uint8_t used;int64_t endpoint;uint64_t owner_pid;uint32_t mask;}subscriber_t;
typedef struct{uint32_t id,category;obs_setting_value_t value;}setting_t;
static subscriber_t subscribers[MAX_SUBSCRIBERS];static uint32_t generation=1;
static obs_app_catalog_t catalog;static obs_pinned_apps_t pins;
static setting_t values[]={
 {OBS_SETTING_APPEARANCE_THEME,OBS_SETTINGS_SUBSCRIBE_APPEARANCE,{OBS_SETTING_TYPE_ENUM,{.enumeration=OBS_THEME_DEFAULT}}},
 {OBS_SETTING_APPEARANCE_TEXT_SCALE,OBS_SETTINGS_SUBSCRIBE_APPEARANCE,{OBS_SETTING_TYPE_U32,{.u32=2}}},
 {OBS_SETTING_DESKTOP_BACKGROUND_MODE,OBS_SETTINGS_SUBSCRIBE_DESKTOP,{OBS_SETTING_TYPE_ENUM,{.enumeration=OBS_BACKGROUND_SUBTLE_BANDS}}},
 {OBS_SETTING_PANEL_PRIMARY_EDGE,OBS_SETTINGS_SUBSCRIBE_PANEL,{OBS_SETTING_TYPE_ENUM,{.enumeration=OBS_PANEL_TOP}}},
 {OBS_SETTING_PANEL_PRIMARY_SIZE,OBS_SETTINGS_SUBSCRIBE_PANEL,{OBS_SETTING_TYPE_U32,{.u32=46}}}
};
typedef struct __attribute__((packed)){
    char magic[8];uint32_t version,size,checksum;
    uint32_t theme,text_scale,background,panel_edge,panel_size,pin_count;
    char pin_ids[OBS_SETTINGS_MAX_PINNED_APPS][OBS_APP_ID_MAX];
}settings_store_t;
#define SETTINGS_STORE_PATH "/state/settings.bin"
#define SETTINGS_STORE_TEMP "/state/settings.new"
static int validate(int index,const obs_setting_value_t*v);
static int same(const char*a,const char*b);
static uint32_t checksum(const void*data,uint32_t size){const uint8_t*p=data;uint32_t h=2166136261U;for(uint32_t i=0;i<size;i++){h^=p[i];h*=16777619U;}return h;}
static void storage_blob(settings_store_t*b){
    for(uint32_t i=0;i<sizeof(*b);i++)((uint8_t*)b)[i]=0;
    const char*m="OBSSET1";for(uint32_t i=0;i<8;i++)b->magic[i]=m[i];b->version=1;b->size=sizeof(*b);
    b->theme=values[0].value.enumeration;b->text_scale=values[1].value.u32;b->background=values[2].value.enumeration;b->panel_edge=values[3].value.enumeration;b->panel_size=values[4].value.u32;b->pin_count=pins.count;
    for(uint32_t i=0;i<pins.count;i++)for(uint32_t n=0;n<OBS_APP_ID_MAX;n++)b->pin_ids[i][n]=pins.ids[i][n];
    b->checksum=0;b->checksum=checksum(b,sizeof(*b));
}
static int storage_load(void){
    settings_store_t b;int64_t fd=sys_open(SETTINGS_STORE_PATH);if(fd<0)return-1;int64_t got=sys_read((int)fd,&b,sizeof(b));char extra;int64_t more=sys_read((int)fd,&extra,1);sys_close((int)fd);
    if(got!=(int64_t)sizeof(b)||more!=0)return-1;
    const char*m="OBSSET1";for(uint32_t i=0;i<8;i++)if(b.magic[i]!=m[i])return-1;if(b.version!=1||b.size!=sizeof(b)||b.pin_count>OBS_SETTINGS_MAX_PINNED_APPS)return-1;uint32_t saved=b.checksum;b.checksum=0;if(checksum(&b,sizeof(b))!=saved)return-1;
    obs_setting_value_t candidate[5]={{OBS_SETTING_TYPE_ENUM,{.enumeration=b.theme}},{OBS_SETTING_TYPE_U32,{.u32=b.text_scale}},{OBS_SETTING_TYPE_ENUM,{.enumeration=b.background}},{OBS_SETTING_TYPE_ENUM,{.enumeration=b.panel_edge}},{OBS_SETTING_TYPE_U32,{.u32=b.panel_size}}};for(int i=0;i<5;i++)if(validate(i,&candidate[i])<0)return-1;
    for(uint32_t i=0;i<b.pin_count;i++){b.pin_ids[i][OBS_APP_ID_MAX-1]=0;if(!b.pin_ids[i][0]||!obs_app_catalog_find_id(&catalog,b.pin_ids[i]))return-1;for(uint32_t n=0;n<i;n++)if(same(b.pin_ids[n],b.pin_ids[i]))return-1;}
    for(int i=0;i<5;i++)values[i].value=candidate[i];
    pins.count=b.pin_count;for(uint32_t i=0;i<pins.count;i++)for(uint32_t n=0;n<OBS_APP_ID_MAX;n++)pins.ids[i][n]=b.pin_ids[i][n];return 0;
}
static int storage_commit_all(void){settings_store_t b;storage_blob(&b);int64_t fd=sys_open_flags(SETTINGS_STORE_TEMP,OS_OPEN_CREATE|OS_OPEN_TRUNC);if(fd<0)return-1;int ok=sys_fd_write((int)fd,&b,sizeof(b))==(int64_t)sizeof(b)&&sys_fd_sync((int)fd)==0;sys_close((int)fd);if(!ok||sys_rename(SETTINGS_STORE_TEMP,SETTINGS_STORE_PATH,1)<0)return-1;return 0;}
static int storage_commit(uint32_t setting_id,const obs_setting_value_t*value){(void)setting_id;(void)value;return storage_commit_all();}
static int storage_commit_pins(const obs_pinned_apps_t*value){(void)value;return storage_commit_all();}
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
static int same(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void pin_respond(int64_t reply,int status,uint32_t index){if(reply<0)return;obs_settings_pin_response_t r={status,OBS_SETTINGS_PROTOCOL_VERSION,generation,pins.count,{0}};if(status==0&&index<pins.count)for(uint32_t i=0;i<sizeof(r.application_id);i++)r.application_id[i]=pins.ids[index][i];os_ipc_try_send((uint64_t)reply,&r,sizeof(r));os_handle_close((uint64_t)reply);}
static void notify_pins(void){setting_t changed={OBS_SETTING_SHELL_PINNED_APPS,OBS_SETTINGS_SUBSCRIBE_SHELL,{OBS_SETTING_TYPE_U32,{.u32=pins.count}}};notify(&changed);}
static void pin_dispatch(const obs_settings_pin_request_t*q,const os_ipc_message_info_t*info,int privileged){
    if(q->version!=OBS_SETTINGS_PROTOCOL_VERSION||q->reserved){pin_respond(info->attached,q->version!=OBS_SETTINGS_PROTOCOL_VERSION?OBS_SETTINGS_BAD_VERSION:OBS_SETTINGS_INVALID_VALUE,0);return;}
    if(q->operation==OBS_SETTINGS_OP_GET_PIN&&!privileged){if(q->index>=pins.count&&pins.count){pin_respond(info->attached,OBS_SETTINGS_INVALID_VALUE,q->index);return;}pin_respond(info->attached,0,q->index);return;}
    if((q->operation==OBS_SETTINGS_OP_PIN||q->operation==OBS_SETTINGS_OP_UNPIN)&&privileged){uint32_t length=0;while(length<sizeof(q->application_id)&&q->application_id[length])length++;if(!length||length==sizeof(q->application_id)||!obs_app_catalog_find_id(&catalog,q->application_id)){pin_respond(info->attached,OBS_SETTINGS_INVALID_VALUE,0);return;}int found=-1;for(uint32_t i=0;i<pins.count;i++)if(same(pins.ids[i],q->application_id)){found=(int)i;break;}
        if(q->operation==OBS_SETTINGS_OP_PIN){if(found>=0){pin_respond(info->attached,0,(uint32_t)found);return;}if(pins.count>=OBS_SETTINGS_MAX_PINNED_APPS){pin_respond(info->attached,OBS_SETTINGS_ERROR,0);return;}for(uint32_t n=0;n<OBS_APP_ID_MAX;n++)pins.ids[pins.count][n]=q->application_id[n];pins.count++;}
        else{if(found<0){pin_respond(info->attached,OBS_SETTINGS_INVALID_VALUE,0);return;}for(uint32_t i=(uint32_t)found;i+1<pins.count;i++)for(uint32_t n=0;n<OBS_APP_ID_MAX;n++)pins.ids[i][n]=pins.ids[i+1][n];pins.count--;for(uint32_t n=0;n<OBS_APP_ID_MAX;n++)pins.ids[pins.count][n]=0;}
        (void)storage_commit_pins(&pins);generation++;if(!generation)generation=1;notify_pins();write(q->operation==OBS_SETTINGS_OP_PIN?"settings: application pinned\n":"settings: application unpinned\n");pin_respond(info->attached,0,0);return;}
    pin_respond(info->attached,OBS_SETTINGS_DENIED,0);
}
static void subscribe(const obs_settings_request_t*q,const os_ipc_message_info_t*info){int slot=-1;if(!q->subscription_mask||(q->subscription_mask&~OBS_SETTINGS_SUBSCRIBE_ALL)){respond(info->attached,OBS_SETTINGS_INVALID_VALUE,0,0);return;}for(int i=0;i<MAX_SUBSCRIBERS;i++)if(!subscribers[i].used){slot=i;break;}if(slot<0){respond(info->attached,OBS_SETTINGS_ERROR,0,0);return;}subscribers[slot]=(subscriber_t){1,info->attached,info->sender_pid,q->subscription_mask};obs_settings_response_t r={0,OBS_SETTINGS_PROTOCOL_VERSION,0,generation,{0}};if(os_ipc_try_send((uint64_t)info->attached,&r,sizeof(r))!=(int64_t)sizeof(r))close_sub(slot);}
static void dispatch(const uint8_t*data,int64_t size,const os_ipc_message_info_t*info,int privileged){
    if(size==(int64_t)sizeof(obs_settings_pin_request_t)){pin_dispatch((const obs_settings_pin_request_t*)data,info,privileged);return;}
    if(size!=(int64_t)sizeof(obs_settings_request_t)){respond(info->attached,OBS_SETTINGS_ERROR,0,0);return;}const obs_settings_request_t*q=(const obs_settings_request_t*)data;
    if(q->version!=OBS_SETTINGS_PROTOCOL_VERSION){respond(info->attached,OBS_SETTINGS_BAD_VERSION,q->setting_id,0);return;}
    if(q->operation==OBS_SETTINGS_OP_SUBSCRIBE&&!privileged){subscribe(q,info);return;}
    if(q->operation==OBS_SETTINGS_OP_QUERY_SUBSCRIBERS&&!privileged){obs_setting_value_t v={OBS_SETTING_TYPE_U32,{.u32=0}};for(int i=0;i<MAX_SUBSCRIBERS;i++)if(subscribers[i].used)v.u32++;respond(info->attached,0,0,&v);return;}
    int index=find(q->setting_id);if(q->operation==OBS_SETTINGS_OP_GET&&!privileged){if(index<0)respond(info->attached,OBS_SETTINGS_UNKNOWN_ID,q->setting_id,0);else respond(info->attached,0,q->setting_id,&values[index].value);return;}
    if(q->operation==OBS_SETTINGS_OP_SET&&privileged){int status=validate(index,&q->value);if(status<0){respond(info->attached,status,q->setting_id,0);return;}if(!same_value(&values[index].value,&q->value)){values[index].value=q->value;(void)storage_commit(q->setting_id,&q->value);generation++;if(!generation)generation=1;notify(&values[index]);if(q->setting_id==OBS_SETTING_APPEARANCE_THEME)write(q->value.enumeration==OBS_THEME_ALTERNATE?"settings: theme -> alternate\n":"settings: theme -> default\n");else if(q->setting_id==OBS_SETTING_PANEL_PRIMARY_EDGE)write(q->value.enumeration==OBS_PANEL_BOTTOM?"settings: panel edge -> bottom\n":"settings: panel edge -> top\n");else if(q->setting_id==OBS_SETTING_PANEL_PRIMARY_SIZE)write("settings: panel size changed\n");else if(q->setting_id==OBS_SETTING_APPEARANCE_TEXT_SCALE)write("settings: text scale changed\n");}respond(info->attached,0,q->setting_id,&values[index].value);return;}
    respond(info->attached,privileged?OBS_SETTINGS_ERROR:OBS_SETTINGS_DENIED,q->setting_id,0);
}
static int poll(int64_t endpoint,int privileged){uint8_t message[64];os_ipc_message_info_t info={-1,0};int64_t n=os_ipc_recv_ex((uint64_t)endpoint,message,sizeof(message),&info,1);if(n==-2)return 0;if(n<0)return 0;dispatch(message,n,&info,privileged);return 1;}
int obsidia_main(void){int64_t authority=os_handle_find(OS_OBJECT_IPC);if(authority<0)return 1;os_handle_set_inheritable((uint64_t)authority,0);if(obs_app_catalog_connect(&catalog)<0)return 2;pins.count=1;const char*default_pin="org.obsidia.window-demo";for(uint32_t i=0;default_pin[i]&&i+1<OBS_APP_ID_MAX;i++)pins.ids[0][i]=default_pin[i];int loaded=storage_load()==0;int64_t endpoint=os_ipc_create();if(endpoint<0||os_service_register_restricted(OBS_SETTINGS_SERVICE_NAME,(uint64_t)endpoint,OS_RIGHT_WRITE|OS_RIGHT_DUP)<0)return 3;write(loaded?"settings: service ready; persistent state loaded\n":"settings: service ready; defaults active\n");for(;;){int worked=0;for(uint32_t i=0;i<32;i++){int n=poll(endpoint,0);worked|=n;if(!n)break;}for(uint32_t i=0;i<16;i++){int n=poll(authority,1);worked|=n;if(!n)break;}sweep();if(worked)sys_yield();else sys_sleep(1);}}
