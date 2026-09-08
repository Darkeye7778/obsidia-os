#pragma once
#include <stdint.h>
#include <obsidia/app_catalog.h>

#define OBS_APP_CATALOG_PROTOCOL_VERSION 1U
#define OBS_APP_CATALOG_OP_SNAPSHOT 1U
#define OBS_APP_CATALOG_SNAPSHOT_MAGIC 0x43415041U
typedef struct {uint32_t version,operation,reserved0,reserved1;} obs_app_catalog_request_t;
typedef struct {int32_t status;uint32_t version,generation,count,record_size;} obs_app_catalog_response_t;
typedef struct {uint32_t magic,version,generation,count;obs_app_metadata_t records[OBS_APP_CATALOG_MAX];} obs_app_catalog_snapshot_t;
enum {OBS_APP_CATALOG_OK=0,OBS_APP_CATALOG_ERROR=-1,OBS_APP_CATALOG_BAD_VERSION=-2};
