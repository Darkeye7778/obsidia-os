#include <obsidia/internal/shell_model.h>
#include <obsidia/process.h>
#include <obsidia/window.h>

static int same(const char*a,const char*b){if(!a||!b)return 0;while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void copy_safe(char*out,uint32_t capacity,const char*in){
    uint32_t i=0;if(!capacity)return;
    for(;in&&in[i]&&i+1<capacity;i++){unsigned char c=(unsigned char)in[i];out[i]=c>=32&&c<=126?(char)c:'?';}
    out[i]=0;
}
static void fallback_label(char*out,const char*image){
    const char*base=image;for(const char*p=image;p&&*p;p++)if(*p=='/'||*p=='\\')base=p+1;
    copy_safe(out,OBS_APP_NAME_MAX,base&&*base?base:"Application");
    for(uint32_t i=0;out[i];i++)if(out[i]=='.'){out[i]=0;break;}
    if(!out[0])copy_safe(out,OBS_APP_NAME_MAX,"Application");
}
static obs_shell_app_group_t*find_identity(obs_shell_model_t*m,const char*identity){
    for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(m->groups[i].used&&same(m->groups[i].application_id,identity))return&m->groups[i];
    return 0;
}
static obs_shell_app_group_t*allocate_group(obs_shell_model_t*m){for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(!m->groups[i].used)return&m->groups[i];return 0;}
static obs_shell_window_t*find_window(obs_shell_model_t*m,uint32_t id,obs_shell_app_group_t**owner){
    for(uint32_t g=0;g<OBS_SHELL_MAX_APP_GROUPS;g++)if(m->groups[g].used)for(uint32_t w=0;w<OBS_SHELL_MAX_WINDOWS_PER_GROUP;w++)if(m->groups[g].windows[w].used&&m->groups[g].windows[w].id==id){if(owner)*owner=&m->groups[g];return&m->groups[g].windows[w];}
    return 0;
}
static void apply_metadata(obs_shell_app_group_t*g,const obs_app_metadata_t*meta){g->icon=meta->icon;copy_safe(g->application_id,sizeof(g->application_id),meta->id);copy_safe(g->image,sizeof(g->image),meta->path);copy_safe(g->display_name,sizeof(g->display_name),meta->display_name);}
int obs_shell_sync_pins(obs_shell_model_t*m,const obs_app_catalog_t*catalog,const obs_pinned_apps_t*pins){
    if(!m||!catalog||!pins||pins->count>OBS_SETTINGS_MAX_PINNED_APPS)return-1;
    /* Validate the complete replacement before changing the live shell model.
     * settingsd already enforces this, but keeping the cache update atomic also
     * makes reconnect/reload failures harmless to the desktop. */
    uint32_t new_groups=0,free_groups=0;
    for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(!m->groups[i].used)free_groups++;
    for(uint32_t i=0;i<pins->count;i++){
        const obs_app_metadata_t*meta=obs_app_catalog_find_id(catalog,pins->ids[i]);
        if(!meta)return-1;
        if(!find_identity(m,meta->id))new_groups++;
        for(uint32_t n=0;n<i;n++)if(same(pins->ids[n],pins->ids[i]))return-1;
    }
    if(new_groups>free_groups)return-1;
    for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(m->groups[i].used){m->groups[i].pinned=0;m->groups[i].pin_order=0xff;}
    for(uint32_t i=0;i<pins->count;i++){const obs_app_metadata_t*meta=obs_app_catalog_find_id(catalog,pins->ids[i]);if(!meta)return-1;obs_shell_app_group_t*g=find_identity(m,meta->id);if(!g){g=allocate_group(m);if(!g)return-1;g->used=1;apply_metadata(g,meta);}g->pinned=1;g->pin_order=(uint8_t)i;}
    for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(m->groups[i].used&&!m->groups[i].pinned&&!m->groups[i].window_count)for(uint32_t n=0;n<sizeof(m->groups[i]);n++)((uint8_t*)&m->groups[i])[n]=0;
    return 0;
}
void obs_shell_model_init(obs_shell_model_t*m,const obs_app_catalog_t*catalog,const obs_pinned_apps_t*pins){
    if(!m)return;
    for(uint32_t i=0;i<sizeof(*m);i++)((uint8_t*)m)[i]=0;
    for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)m->groups[i].pin_order=0xff;
    if(catalog&&pins)(void)obs_shell_sync_pins(m,catalog,pins);
}
int obs_shell_window_added(obs_shell_model_t*m,const obs_app_catalog_t*catalog,uint32_t id,uint64_t owner_pid,const char*title,uint32_t state){
    obs_process_info_t info;if(os_process_info(owner_pid,&info)<0)return-1;
    return obs_shell_window_added_identity(m,catalog,id,owner_pid,info.image,title,state);
}
int obs_shell_window_added_identity(obs_shell_model_t*m,const obs_app_catalog_t*catalog,uint32_t id,uint64_t owner_pid,const char*image,const char*title,uint32_t state){
    if(!m||!id||!owner_pid||!image||!*image||state>OBS_WINDOW_MAXIMIZED||find_window(m,id,0))return-1;
    const obs_app_metadata_t*meta=obs_app_catalog_find_path(catalog,image);const char*identity=meta?meta->id:image;
    obs_shell_app_group_t*g=find_identity(m,identity);
    if(!g){g=allocate_group(m);if(!g)return-1;g->used=1;g->pin_order=0xff;if(meta)apply_metadata(g,meta);else{g->icon=OBS_ICON_APPLICATION;copy_safe(g->application_id,sizeof(g->application_id),identity);copy_safe(g->image,sizeof(g->image),image);fallback_label(g->display_name,image);}}
    uint32_t slot=OBS_SHELL_MAX_WINDOWS_PER_GROUP;for(uint32_t i=0;i<OBS_SHELL_MAX_WINDOWS_PER_GROUP;i++)if(!g->windows[i].used){slot=i;break;}
    if(slot==OBS_SHELL_MAX_WINDOWS_PER_GROUP)return-1;
    obs_shell_window_t*w=&g->windows[slot];w->used=1;w->id=id;w->owner_pid=owner_pid;w->state=state;w->mru=++m->mru_serial;copy_safe(w->title,32,title);g->window_count++;
    return 0;
}
int obs_shell_window_removed(obs_shell_model_t*m,uint32_t id){
    obs_shell_app_group_t*g=0;obs_shell_window_t*w=find_window(m,id,&g);if(!w)return-1;
    for(uint32_t i=0;i<sizeof(*w);i++)((uint8_t*)w)[i]=0;
    if(g->window_count)g->window_count--;
    if(!g->window_count&&!g->pinned)for(uint32_t i=0;i<sizeof(*g);i++)((uint8_t*)g)[i]=0;
    return 0;
}
int obs_shell_window_state(obs_shell_model_t*m,uint32_t id,uint32_t state){obs_shell_window_t*w=find_window(m,id,0);if(!w||state>OBS_WINDOW_MAXIMIZED)return-1;w->state=state;return 0;}
int obs_shell_window_focused(obs_shell_model_t*m,uint32_t id){
    if(!m)return-1;
    obs_shell_window_t*target=0;
    for(uint32_t g=0;g<OBS_SHELL_MAX_APP_GROUPS;g++)if(m->groups[g].used)for(uint32_t w=0;w<OBS_SHELL_MAX_WINDOWS_PER_GROUP;w++)if(m->groups[g].windows[w].used){m->groups[g].windows[w].focused=0;if(m->groups[g].windows[w].id==id)target=&m->groups[g].windows[w];}
    if(id&&!target)return-1;
    if(target){target->focused=1;target->mru=++m->mru_serial;}
    return 0;
}
uint32_t obs_shell_group_count(const obs_shell_model_t*m){uint32_t n=0;if(m)for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(m->groups[i].used)n++;return n;}
obs_shell_app_group_t*obs_shell_group_at(obs_shell_model_t*m,uint32_t visible_index){if(!m)return 0;for(uint32_t order=0;order<OBS_SETTINGS_MAX_PINNED_APPS;order++)for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(m->groups[i].used&&m->groups[i].pinned&&m->groups[i].pin_order==order){if(!visible_index)return&m->groups[i];visible_index--;}for(uint32_t i=0;i<OBS_SHELL_MAX_APP_GROUPS;i++)if(m->groups[i].used&&!m->groups[i].pinned){if(!visible_index)return&m->groups[i];visible_index--;}return 0;}
int obs_shell_group_all_minimized(const obs_shell_app_group_t*g){if(!g||!g->window_count)return 0;for(uint32_t i=0;i<OBS_SHELL_MAX_WINDOWS_PER_GROUP;i++)if(g->windows[i].used&&g->windows[i].state!=OBS_WINDOW_MINIMIZED)return 0;return 1;}
uint32_t obs_shell_group_choose_window(const obs_shell_app_group_t*g){
    if(!g||!g->window_count)return 0;
    int focused=-1;uint32_t best=0;uint64_t best_mru=0;
    for(uint32_t i=0;i<OBS_SHELL_MAX_WINDOWS_PER_GROUP;i++)if(g->windows[i].used){if(g->windows[i].focused)focused=(int)i;if(g->windows[i].mru>=best_mru){best=i;best_mru=g->windows[i].mru;}}
    if(focused>=0&&g->window_count>1)for(uint32_t step=1;step<=OBS_SHELL_MAX_WINDOWS_PER_GROUP;step++){uint32_t i=((uint32_t)focused+step)%OBS_SHELL_MAX_WINDOWS_PER_GROUP;if(g->windows[i].used)return g->windows[i].id;}
    return g->windows[best].id;
}
