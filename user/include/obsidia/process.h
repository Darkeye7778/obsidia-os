#pragma once
#include <stdint.h>

#define OBS_PROCESS_IMAGE_MAX 32U
typedef struct {
    uint64_t pid;
    uint64_t parent_pid;
    char image[OBS_PROCESS_IMAGE_MAX];
} obs_process_info_t;

/* Returns only bounded, non-sensitive identity metadata for a live process. */
int os_process_info(uint64_t pid,obs_process_info_t* info);
