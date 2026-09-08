#pragma once
#include <stdint.h>
#include <obsidia/app_catalog.h>
#include <obsidia/settings.h>

#define OBS_SHELL_MAX_APP_GROUPS 16U
#define OBS_SHELL_MAX_WINDOWS_PER_GROUP 16U

typedef struct {
    uint8_t used,focused;
    uint32_t id,state;
    uint64_t owner_pid,mru;
    char title[32];
} obs_shell_window_t;

typedef struct {
    uint8_t used,pinned,pin_order;
    char application_id[OBS_APP_ID_MAX],image[OBS_APP_PATH_MAX],display_name[OBS_APP_NAME_MAX];
    obs_icon_id_t icon;
    uint32_t window_count;
    obs_shell_window_t windows[OBS_SHELL_MAX_WINDOWS_PER_GROUP];
} obs_shell_app_group_t;

typedef struct {
    obs_shell_app_group_t groups[OBS_SHELL_MAX_APP_GROUPS];
    uint64_t mru_serial;
} obs_shell_model_t;

void obs_shell_model_init(obs_shell_model_t*model,const obs_app_catalog_t*catalog,const obs_pinned_apps_t*pins);
int obs_shell_sync_pins(obs_shell_model_t*model,const obs_app_catalog_t*catalog,const obs_pinned_apps_t*pins);
int obs_shell_window_added(obs_shell_model_t*model,const obs_app_catalog_t*catalog,uint32_t id,uint64_t owner_pid,const char*title,uint32_t state);
int obs_shell_window_added_identity(obs_shell_model_t*model,const obs_app_catalog_t*catalog,uint32_t id,uint64_t owner_pid,const char*image,const char*title,uint32_t state);
int obs_shell_window_removed(obs_shell_model_t*model,uint32_t id);
int obs_shell_window_state(obs_shell_model_t*model,uint32_t id,uint32_t state);
int obs_shell_window_focused(obs_shell_model_t*model,uint32_t id);
uint32_t obs_shell_group_count(const obs_shell_model_t*model);
obs_shell_app_group_t*obs_shell_group_at(obs_shell_model_t*model,uint32_t visible_index);
uint32_t obs_shell_group_choose_window(const obs_shell_app_group_t*group);
int obs_shell_group_all_minimized(const obs_shell_app_group_t*group);
