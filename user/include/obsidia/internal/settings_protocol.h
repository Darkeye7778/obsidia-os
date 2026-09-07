#pragma once
#include <stdint.h>
#include <obsidia/settings.h>

#define OBS_SETTINGS_OP_GET 1U
#define OBS_SETTINGS_OP_SET 2U
#define OBS_SETTINGS_OP_SUBSCRIBE 3U
#define OBS_SETTINGS_OP_QUERY_SUBSCRIBERS 4U

typedef struct {
    uint32_t version,operation,setting_id,subscription_mask;
    obs_setting_value_t value;
} obs_settings_request_t;

typedef struct {
    int32_t status;
    uint32_t version,setting_id,generation;
    obs_setting_value_t value;
} obs_settings_response_t;

