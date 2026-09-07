#include <obsidia/app_metadata.h>
#include <obsidia/app.h>

static const obs_app_metadata_t applications[]={
    {"org.obsidia.window-demo","window-demo" OBS_NATIVE_EXEC_EXTENSION,"Window Demo",OBS_ICON_WINDOW_DEMO,0},
    {"org.obsidia.settings-demo","settings-demo" OBS_NATIVE_EXEC_EXTENSION,"Appearance",OBS_ICON_SETTINGS,OBS_APP_CAP_SETTINGS_WRITE}
};
static int same(const char*a,const char*b){if(!a||!b)return 0;while(*a&&*a==*b){a++;b++;}return *a==*b;}
const obs_app_metadata_t *obs_app_metadata_find(const char *path){for(uint32_t i=0;i<sizeof(applications)/sizeof(applications[0]);i++)if(same(path,applications[i].path))return &applications[i];return 0;}
