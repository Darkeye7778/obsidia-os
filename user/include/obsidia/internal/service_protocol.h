#pragma once
#include <stdint.h>
#define OBS_SERVICE_REGISTER 1U
#define OBS_SERVICE_CONNECT 2U
typedef struct { uint32_t operation; uint32_t published_rights; char name[32]; } obs_service_request_t;
