#include "syscall.h"
#include <obsidia/app.h>
#include <obsidia/settings.h>
#include <obsidia/theme.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/service.h>
#include <obsidia/internal/settings_protocol.h>

static int fail(int code){
    static const char text[]="settings-test: FAILED\n";
    sys_fd_write(1,text,sizeof(text)-1);
    return code;
}
static void trace(const char*s){uint32_t n=0;while(s[n])n++;sys_fd_write(1,s,n);}
static obs_setting_value_t val(uint32_t type,uint32_t value){
    obs_setting_value_t v={0};v.type=type;v.u32=value;return v;
}
static int raw_status(uint32_t version,uint32_t operation){
    int64_t service=os_service_connect(OBS_SETTINGS_SERVICE_NAME),reply=os_ipc_create();
    if(service<0||reply<0)return 99;
    obs_settings_request_t q={version,operation,OBS_SETTING_APPEARANCE_THEME,0,
                              val(OBS_SETTING_TYPE_ENUM,OBS_THEME_DEFAULT)};
    obs_settings_response_t r={0};
    int ok=os_ipc_send_handle((uint64_t)service,&q,sizeof(q),(uint64_t)reply,OS_RIGHT_WRITE)==(int64_t)sizeof(q)&&
           os_ipc_recv((uint64_t)reply,&r,sizeof(r))==(int64_t)sizeof(r);
    os_handle_close((uint64_t)service);os_handle_close((uint64_t)reply);
    return ok?r.status:99;
}

int obsidia_main(void){
    int64_t authority=os_handle_find(OS_OBJECT_IPC);
    if(authority<0)return fail(1);
    os_handle_set_inheritable((uint64_t)authority,0);
    obs_settings_t settings,reader;
    if(obs_settings_connect_with_authority(&settings,(uint64_t)authority)<0||obs_settings_connect(&reader)<0)return fail(2);
    obs_setting_value_t got={0};
    if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_THEME,&got)<0||got.type!=OBS_SETTING_TYPE_ENUM||got.enumeration!=OBS_THEME_DEFAULT)return fail(3);
    if(obs_settings_get(&settings,OBS_SETTING_PANEL_PRIMARY_EDGE,&got)<0||got.enumeration!=OBS_PANEL_TOP)return fail(4);
    if(obs_settings_get(&settings,OBS_SETTING_PANEL_PRIMARY_SIZE,&got)<0||got.u32!=46)return fail(5);
    if(obs_settings_get(&settings,0x9999,&got)!=OBS_SETTINGS_UNKNOWN_ID)return fail(6);

    obs_setting_value_t v=val(OBS_SETTING_TYPE_U32,OBS_THEME_ALTERNATE);
    if(obs_settings_set(&settings,OBS_SETTING_APPEARANCE_THEME,&v)!=OBS_SETTINGS_WRONG_TYPE)return fail(7);
    v=val(OBS_SETTING_TYPE_ENUM,99);
    if(obs_settings_set(&settings,OBS_SETTING_APPEARANCE_THEME,&v)!=OBS_SETTINGS_INVALID_VALUE)return fail(8);
    v=val(OBS_SETTING_TYPE_ENUM,OBS_PANEL_LEFT);
    if(obs_settings_set(&settings,OBS_SETTING_PANEL_PRIMARY_EDGE,&v)!=OBS_SETTINGS_INVALID_VALUE)return fail(9);
    v=val(OBS_SETTING_TYPE_U32,31);
    if(obs_settings_set(&settings,OBS_SETTING_PANEL_PRIMARY_SIZE,&v)!=OBS_SETTINGS_INVALID_VALUE)return fail(10);
    if(obs_settings_set(&reader,OBS_SETTING_APPEARANCE_THEME,&v)!=OBS_SETTINGS_DENIED||
       raw_status(OBS_SETTINGS_PROTOCOL_VERSION,OBS_SETTINGS_OP_SET)!=OBS_SETTINGS_DENIED)return fail(11);
    if(raw_status(OBS_SETTINGS_PROTOCOL_VERSION+1,OBS_SETTINGS_OP_GET)!=OBS_SETTINGS_BAD_VERSION)return fail(12);

    if(obs_settings_subscribe(&settings,OBS_SETTINGS_SUBSCRIBE_ALL)<0)return fail(13);
    v=val(OBS_SETTING_TYPE_ENUM,OBS_THEME_ALTERNATE);
    if(obs_settings_set(&settings,OBS_SETTING_APPEARANCE_THEME,&v)<0)return fail(14);
    obs_settings_event_t event={0};
    for(uint32_t i=0;i<20&&obs_settings_try_next(&settings,&event)==0;i++)sys_sleep(1);
    if(event.setting_id!=OBS_SETTING_APPEARANCE_THEME||event.value.enumeration!=OBS_THEME_ALTERNATE)return fail(15);
    v=val(OBS_SETTING_TYPE_ENUM,OBS_PANEL_BOTTOM);
    if(obs_settings_set(&settings,OBS_SETTING_PANEL_PRIMARY_EDGE,&v)<0)return fail(16);
    v=val(OBS_SETTING_TYPE_U32,54);
    if(obs_settings_set(&settings,OBS_SETTING_PANEL_PRIMARY_SIZE,&v)<0)return fail(17);
    v=val(OBS_SETTING_TYPE_U32,3);
    if(obs_settings_set(&settings,OBS_SETTING_APPEARANCE_TEXT_SCALE,&v)<0)return fail(18);

    obs_settings_close(&settings);
    uint32_t count=99;
    for(uint32_t i=0;i<10;i++){sys_sleep(1);if(obs_settings_query_subscribers(&reader,&count)==0&&count==0)break;}
    if(count)return fail(19);
    trace("settings-test: local subscription reclaimed\n");
    int64_t child=obs_app_launch("settings-subscriber" OBS_NATIVE_EXEC_EXTENSION);
    if(child<0)return fail(20);
    sys_sleep(4);
    if(obs_settings_query_subscribers(&reader,&count)<0||count!=1)return fail(21);
    trace("settings-test: child subscribed\n");
    int64_t status=0;sys_wait((uint64_t)child,&status);
    trace("settings-test: child exited\n");
    for(uint32_t i=0;i<10;i++){sys_sleep(1);obs_settings_query_subscribers(&reader,&count);if(!count)break;}
    if(count)return fail(22);
    trace("settings-test: dead child subscription reclaimed\n");

    if(obs_settings_connect_with_authority(&settings,(uint64_t)authority)<0)return fail(23);
    v=val(OBS_SETTING_TYPE_ENUM,OBS_THEME_DEFAULT);
    if(obs_settings_set(&settings,OBS_SETTING_APPEARANCE_THEME,&v)<0)return fail(24);
    v=val(OBS_SETTING_TYPE_ENUM,OBS_PANEL_TOP);
    if(obs_settings_set(&settings,OBS_SETTING_PANEL_PRIMARY_EDGE,&v)<0)return fail(25);
    v=val(OBS_SETTING_TYPE_U32,46);
    if(obs_settings_set(&settings,OBS_SETTING_PANEL_PRIMARY_SIZE,&v)<0)return fail(26);
    v=val(OBS_SETTING_TYPE_U32,2);
    if(obs_settings_set(&settings,OBS_SETTING_APPEARANCE_TEXT_SCALE,&v)<0)return fail(27);
    obs_settings_close(&settings);obs_settings_close(&reader);
    static const char passed[]="settings-test: defaults, typed validation, authority, versioning, notifications and dead-subscriber cleanup passed\n";
    sys_fd_write(1,passed,sizeof(passed)-1);
    return 0;
}
