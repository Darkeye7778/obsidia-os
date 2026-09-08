#pragma once
#include <stdint.h>
#include <obsidia/app_metadata.h>

#define OBS_APP_CATALOG_MAX 32U
#define OBS_APP_CATALOG_SERVICE_NAME "applications"

typedef struct {uint64_t service_handle;uint32_t generation,count;obs_app_metadata_t records[OBS_APP_CATALOG_MAX];} obs_app_catalog_t;
int obs_app_catalog_connect(obs_app_catalog_t *catalog);
int obs_app_catalog_refresh(obs_app_catalog_t *catalog);
void obs_app_catalog_close(obs_app_catalog_t *catalog);
uint32_t obs_app_catalog_count(const obs_app_catalog_t *catalog);
const obs_app_metadata_t *obs_app_catalog_at(const obs_app_catalog_t *catalog,uint32_t index);
const obs_app_metadata_t *obs_app_catalog_find_id(const obs_app_catalog_t *catalog,const char *id);
const obs_app_metadata_t *obs_app_catalog_find_path(const obs_app_catalog_t *catalog,const char *path);
