#include <obsidia/settings.h>
#include "syscall.h"
int obsidia_main(void){obs_settings_t settings;if(obs_settings_connect(&settings)<0||obs_settings_subscribe(&settings,OBS_SETTINGS_SUBSCRIBE_APPEARANCE)<0)return 1;static const char ready[]="settings-subscriber: subscribed\n";sys_fd_write(1,ready,sizeof(ready)-1);sys_sleep(12);return 0;}
