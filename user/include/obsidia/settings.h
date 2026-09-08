#pragma once
#include <stdint.h>
#include <obsidia/app_metadata.h>

#define OBS_SETTINGS_PROTOCOL_VERSION 1U
#define OBS_SETTINGS_SERVICE_NAME "settings"

typedef enum {
    OBS_SETTING_TYPE_BOOL=1,OBS_SETTING_TYPE_U32=2,OBS_SETTING_TYPE_I32=3,
    OBS_SETTING_TYPE_ENUM=4,OBS_SETTING_TYPE_COLOR=5,OBS_SETTING_TYPE_STRING=6
} obs_setting_type_t;

typedef enum {
    OBS_SETTING_APPEARANCE_THEME=0x0101,
    OBS_SETTING_APPEARANCE_TEXT_SCALE=0x0102,
    OBS_SETTING_DESKTOP_BACKGROUND_MODE=0x0201,
    OBS_SETTING_PANEL_PRIMARY_EDGE=0x0301,
    OBS_SETTING_PANEL_PRIMARY_SIZE=0x0302,
    OBS_SETTING_SHELL_PINNED_APPS=0x0401
} obs_setting_id_t;

#define OBS_SETTINGS_SUBSCRIBE_APPEARANCE (1U<<0)
#define OBS_SETTINGS_SUBSCRIBE_DESKTOP    (1U<<1)
#define OBS_SETTINGS_SUBSCRIBE_PANEL      (1U<<2)
#define OBS_SETTINGS_SUBSCRIBE_SHELL      (1U<<3)
#define OBS_SETTINGS_SUBSCRIBE_ALL        0xfU
#define OBS_SETTINGS_MAX_PINNED_APPS      8U

typedef struct {
    uint32_t type;
    union {uint32_t boolean,u32,enumeration,color;int32_t i32;char string[32];};
} obs_setting_value_t;

typedef struct {uint32_t setting_id,generation;obs_setting_value_t value;} obs_settings_event_t;
typedef struct {uint64_t service_handle,event_handle,authority_handle;uint32_t subscription_mask;} obs_settings_t;
typedef struct {uint32_t count;char ids[OBS_SETTINGS_MAX_PINNED_APPS][OBS_APP_ID_MAX];} obs_pinned_apps_t;

enum {OBS_SETTINGS_OK=0,OBS_SETTINGS_ERROR=-1,OBS_SETTINGS_BAD_VERSION=-2,
      OBS_SETTINGS_UNKNOWN_ID=-3,OBS_SETTINGS_WRONG_TYPE=-4,
      OBS_SETTINGS_INVALID_VALUE=-5,OBS_SETTINGS_DENIED=-6};

int obs_settings_connect(obs_settings_t*settings);
int obs_settings_connect_with_authority(obs_settings_t*settings,uint64_t authority_handle);
int obs_settings_get(obs_settings_t*settings,uint32_t id,obs_setting_value_t*value);
int obs_settings_set(obs_settings_t*settings,uint32_t id,const obs_setting_value_t*value);
int obs_settings_subscribe(obs_settings_t*settings,uint32_t category_mask);
int obs_settings_try_next(obs_settings_t*settings,obs_settings_event_t*event);
int obs_settings_query_subscribers(obs_settings_t*settings,uint32_t*count);
int obs_settings_get_pinned_apps(obs_settings_t*settings,obs_pinned_apps_t*pins);
int obs_settings_pin_app(obs_settings_t*settings,const char*application_id);
int obs_settings_unpin_app(obs_settings_t*settings,const char*application_id);
void obs_settings_close(obs_settings_t*settings);
