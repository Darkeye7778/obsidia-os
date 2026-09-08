#pragma once
#include <stdint.h>
#include <obsidia/gfx.h>

#define OBS_APP_ID_MAX 48U
#define OBS_APP_PATH_MAX 64U
#define OBS_APP_NAME_MAX 40U
#define OBS_APP_VERSION_MAX 16U
#define OBS_APP_MANIFEST_VERSION 1U
#define OBS_APP_CAP_SETTINGS_WRITE (1U<<0)

typedef struct {
    char id[OBS_APP_ID_MAX];
    char path[OBS_APP_PATH_MAX];
    char display_name[OBS_APP_NAME_MAX];
    char version[OBS_APP_VERSION_MAX];
    obs_icon_id_t icon;
    uint32_t capabilities;
} obs_app_metadata_t;

/* Descriptive metadata only: privileged capabilities are assigned by appd's
 * trusted policy after parsing, never by a manifest. */
int obs_app_manifest_parse(const char *data,uint32_t size,obs_app_metadata_t *record);
