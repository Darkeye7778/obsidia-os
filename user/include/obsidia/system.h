#pragma once
#include <stdint.h>

typedef enum {
    OBS_SYSTEM_POWER_OFF=1,
    OBS_SYSTEM_REBOOT=2
} obs_system_action_t;

/* Requires the generic system-control capability, normally retained by init
   and deliberately delegated to a future session/power service. */
int os_system_control(obs_system_action_t action);

