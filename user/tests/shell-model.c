#include "syscall.h"
#include <obsidia/app.h>
#include <obsidia/app_catalog.h>
#include <obsidia/app_metadata.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/internal/shell_model.h>
#include <obsidia/window.h>

static int fail(int n){return n;}
int obsidia_main(void){
    obs_app_catalog_t catalog={0};catalog.count=2;catalog.records[0]=(obs_app_metadata_t){"org.obsidia.window-demo","window-demo.obsx","Window Demo","1.0",OBS_ICON_WINDOW_DEMO,0};catalog.records[1]=(obs_app_metadata_t){"org.obsidia.settings-demo","settings-demo.obsx","Appearance","1.0",OBS_ICON_SETTINGS,OBS_APP_CAP_SETTINGS_WRITE};obs_pinned_apps_t pins={1,{"org.obsidia.window-demo"}};
    if(!obs_app_catalog_find_path(&catalog,"window-demo" OBS_NATIVE_EXEC_EXTENSION)||!obs_app_catalog_find_id(&catalog,"org.obsidia.settings-demo")||obs_app_catalog_at(&catalog,2))return fail(1);
    obs_shell_model_t model;obs_shell_model_init(&model,&catalog,&pins);
    obs_shell_app_group_t*g=obs_shell_group_at(&model,0);if(obs_shell_group_count(&model)!=1||!g||!g->pinned||g->window_count)return fail(2);
    pins.count=2;for(uint32_t i=0;i<OBS_APP_ID_MAX;i++)pins.ids[1][i]=catalog.records[1].id[i];if(obs_shell_sync_pins(&model,&catalog,&pins)<0||obs_shell_group_count(&model)!=2||!obs_shell_group_at(&model,1)->pinned)return fail(15);pins.count=1;if(obs_shell_sync_pins(&model,&catalog,&pins)<0||obs_shell_group_count(&model)!=1)return fail(16);
    if(obs_shell_window_added_identity(&model,&catalog,33,100,"window-demo.obsx","One",OBS_WINDOW_NORMAL)<0||obs_shell_window_added_identity(&model,&catalog,65,101,"window-demo.obsx","Two",OBS_WINDOW_NORMAL)<0||obs_shell_window_added_identity(&model,&catalog,97,100,"window-demo.obsx","Three",OBS_WINDOW_NORMAL)<0)return fail(3);
    g=obs_shell_group_at(&model,0);if(obs_shell_group_count(&model)!=1||g->window_count!=3)return fail(4);
    if(obs_shell_window_focused(&model,33)<0||obs_shell_group_choose_window(g)!=65)return fail(5);
    if(obs_shell_window_state(&model,33,OBS_WINDOW_MINIMIZED)<0||obs_shell_window_state(&model,65,OBS_WINDOW_MINIMIZED)<0||obs_shell_window_state(&model,97,OBS_WINDOW_MINIMIZED)<0||!obs_shell_group_all_minimized(g))return fail(6);
    if(obs_shell_window_removed(&model,999)==0||g->window_count!=3)return fail(7);
    if(obs_shell_window_removed(&model,33)<0||obs_shell_window_removed(&model,65)<0||obs_shell_window_removed(&model,97)<0||!g->used||g->window_count)return fail(8);
    if(obs_shell_window_added_identity(&model,&catalog,129,102,"window-demo.obsx","Reused",OBS_WINDOW_NORMAL)<0||obs_shell_window_removed(&model,97)==0||g->window_count!=1||obs_shell_window_removed(&model,129)<0)return fail(9);
    if(obs_shell_window_added_identity(&model,&catalog,161,200,"settings-demo.obsx","Appearance",OBS_WINDOW_NORMAL)<0||obs_shell_group_count(&model)!=2)return fail(10);
    pins.count=2;
    for(uint32_t i=0;i<OBS_APP_ID_MAX;i++)pins.ids[1][i]=catalog.records[1].id[i];
    if(obs_shell_sync_pins(&model,&catalog,&pins)<0||!obs_shell_group_at(&model,1)->pinned)return fail(17);
    pins.count=1;
    if(obs_shell_sync_pins(&model,&catalog,&pins)<0||obs_shell_group_at(&model,1)->pinned||obs_shell_window_removed(&model,161)<0||obs_shell_group_count(&model)!=1)return fail(18);
    if(obs_shell_window_added_identity(&model,&catalog,193,300,"mystery.obsx","Mystery A",OBS_WINDOW_NORMAL)<0||obs_shell_window_added_identity(&model,&catalog,225,301,"mystery.obsx","Mystery B",OBS_WINDOW_NORMAL)<0)return fail(11);
    g=obs_shell_group_at(&model,1);if(!g||g->window_count!=2||g->pinned)return fail(12);
    obs_shell_model_init(&model,&catalog,0);char image[12]="unknown-A";for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++){image[8]=(char)('A'+i);if(obs_shell_window_added_identity(&model,&catalog,32U*(i+1)+1,500+i,image,"Unknown",OBS_WINDOW_NORMAL)<0)return fail(13);}image[8]='Z';if(obs_shell_window_added_identity(&model,&catalog,9999,999,image,"Overflow",OBS_WINDOW_NORMAL)==0||obs_shell_group_count(&model)!=OBS_SHELL_MAX_APP_GROUPS)return fail(14);
    static const char passed[]="shell-model: metadata, pinned/running, grouping, cycling, stale removal and capacity passed\n";sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
