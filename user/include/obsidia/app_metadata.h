#pragma once
#include <obsidia/gfx.h>

#define OBS_APP_CAP_SETTINGS_WRITE (1U<<0)
typedef struct {const char *id,*path,*display_name;obs_icon_id_t icon;uint32_t capabilities;} obs_app_metadata_t;
const obs_app_metadata_t *obs_app_metadata_find(const char *path);
