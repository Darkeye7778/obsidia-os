#include <obsidia/app.h>
#include "syscall.h"

obs_exec_format_t obs_exec_detect(const char* path) {
    int64_t format=os_exec_detect(path);
    if(format<OBS_EXEC_UNKNOWN||format>OBS_EXEC_PE32_PLUS)return OBS_EXEC_UNKNOWN;
    return (obs_exec_format_t)format;
}

int64_t obs_app_launch(const char* path) {
    if(obs_exec_detect(path)==OBS_EXEC_UNKNOWN)return -1;
    return sys_spawn(path);
}
