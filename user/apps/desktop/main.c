#include "syscall.h"
#include <obsidia/app.h>
#include <obsidia/app_metadata.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/gfx.h>
#include <obsidia/input.h>
#include <obsidia/theme.h>
#include <obsidia/settings.h>
#include <obsidia/window.h>
#include <obsidia/internal/desktop.h>

#define MAX_TASKS 16
#define TARGET_NONE (-1)
#define TARGET_LAUNCHER (-2)
#define TARGET_SHORTCUT_BASE (-100)

typedef struct{uint8_t used,focused;uint32_t id,state;char title[32];}task_entry_t;
static task_entry_t tasks[MAX_TASKS];
static const obs_theme_t*theme;
static const obs_desktop_layout_t*layout;
static obs_theme_t active_theme;
static obs_desktop_layout_t active_layout;
static obs_settings_t settings;
static int64_t settings_authority=-1;
static uint32_t theme_id=OBS_THEME_DEFAULT,text_scale=2;
static obs_canvas_t canvas;
static int hover_target=TARGET_NONE,pressed_target=TARGET_NONE;

static void copy_title(char*out,const char*in){uint32_t i=0;for(;i<31&&in&&in[i];i++){unsigned char c=(unsigned char)in[i];out[i]=c>=32&&c<=126?(char)c:'?';}out[i]=0;}
static void draw_background(void){
    obs_fill_rect(&canvas,0,0,(int32_t)canvas.width,(int32_t)canvas.height,theme->colors.desktop_background);
    if(layout->background_mode==OBS_BACKGROUND_SUBTLE_BANDS){uint32_t band=canvas.height/5;if(!band)band=1;for(uint32_t i=0;i<5;i++)if(i&1)obs_fill_rect(&canvas,0,(int32_t)(i*band),(int32_t)canvas.width,(int32_t)band,theme->colors.desktop_background_secondary);obs_fill_rect(&canvas,(int32_t)(canvas.width*2/3),0,(int32_t)(canvas.width/3),(int32_t)canvas.height,theme->colors.desktop_background);}
}
static int panel_geometry(uint32_t index,obs_work_area_t*g){return index<layout->panel_count?obs_panel_geometry(&layout->panels[index],canvas.width,canvas.height,g):-1;}
static int32_t module_origin_x(const obs_panel_config_t*panel,uint32_t module){int32_t x=(int32_t)theme->metrics.spacing_medium;for(uint32_t i=0;i<module;i++)x+=(int32_t)panel->modules[i].extent;return x;}
static void render_launcher(const obs_work_area_t*g,const obs_panel_config_t*panel,uint32_t module){
    int32_t x=g->x+module_origin_x(panel,module),y=g->y+(int32_t)(g->height-theme->metrics.task_entry_height)/2,w=(int32_t)panel->modules[module].extent-(int32_t)theme->metrics.spacing_medium;
    uint32_t color=pressed_target==TARGET_LAUNCHER?theme->colors.panel_item_pressed:(hover_target==TARGET_LAUNCHER?theme->colors.panel_item_hover:theme->colors.panel_item);
    obs_fill_rect(&canvas,x,y,w,(int32_t)theme->metrics.task_entry_height,color);obs_draw_icon(&canvas,x+6,y+4,24,OBS_ICON_SYSTEM,theme->colors.text_primary,theme->colors.accent_primary);obs_draw_text_clipped(&canvas,x+38,y+9,(uint32_t)(w-44),"Obsidia",theme->colors.text_primary,theme->metrics.text_scale);
}
static int32_t tasks_origin(const obs_panel_config_t*panel){for(uint32_t i=0;i<panel->module_count;i++)if(panel->modules[i].type==OBS_PANEL_MODULE_RUNNING_APPS)return module_origin_x(panel,i);return 0;}
static void task_rect(const obs_work_area_t*g,const obs_panel_config_t*panel,uint32_t slot,obs_work_area_t*r){uint32_t h=theme->metrics.task_entry_height,w=theme->metrics.task_entry_width;r->x=g->x+tasks_origin(panel)+(int32_t)(slot*(w+theme->metrics.spacing_small));r->y=g->y+(int32_t)(g->height-h)/2;r->width=w;r->height=h;}
static void render_tasks(const obs_work_area_t*g,const obs_panel_config_t*panel){
    for(uint32_t i=0;i<MAX_TASKS;i++)if(tasks[i].used){obs_work_area_t r;task_rect(g,panel,i,&r);if(r.x>=(int32_t)(g->x+g->width))continue;uint32_t color=pressed_target==(int)i?theme->colors.panel_item_pressed:(hover_target==(int)i?theme->colors.panel_item_hover:theme->colors.panel_item);if(tasks[i].state==OBS_WINDOW_MINIMIZED)color=theme->colors.titlebar_inactive;obs_fill_rect(&canvas,r.x,r.y,(int32_t)r.width,(int32_t)r.height,color);if(tasks[i].focused)obs_stroke_rect(&canvas,r.x,r.y,(int32_t)r.width,(int32_t)r.height,2,theme->colors.accent_primary);else obs_hline(&canvas,r.x,r.y+(int32_t)r.height-1,(int32_t)r.width,theme->colors.panel_border);obs_draw_icon(&canvas,r.x+6,r.y+6,20,OBS_ICON_APPLICATION,theme->colors.text_secondary,theme->colors.accent_secondary);obs_draw_text_clipped(&canvas,r.x+32,r.y+9,r.width>40?r.width-40:0,tasks[i].title[0]?tasks[i].title:"Application",tasks[i].state==OBS_WINDOW_MINIMIZED?theme->colors.text_disabled:theme->colors.text_primary,theme->metrics.text_scale);}
}
static void render_panel(uint32_t index){obs_work_area_t g;if(panel_geometry(index,&g)<0)return;const obs_panel_config_t*panel=&layout->panels[index];obs_fill_rect(&canvas,g.x,g.y,(int32_t)g.width,(int32_t)g.height,theme->colors.panel_background);if(panel->edge==OBS_PANEL_TOP)obs_hline(&canvas,g.x,g.y+(int32_t)g.height-1,(int32_t)g.width,theme->colors.panel_border);else if(panel->edge==OBS_PANEL_BOTTOM)obs_hline(&canvas,g.x,g.y,(int32_t)g.width,theme->colors.panel_border);for(uint32_t i=0;i<panel->module_count&&i<OBS_LAYOUT_MAX_MODULES;i++){if(panel->modules[i].type==OBS_PANEL_MODULE_LAUNCHER)render_launcher(&g,panel,i);else if(panel->modules[i].type==OBS_PANEL_MODULE_RUNNING_APPS)render_tasks(&g,panel);}}
static void shortcut_rect(uint32_t index,obs_work_area_t*r){const obs_shortcut_config_t*s=&layout->shortcuts[index];*r=(obs_work_area_t){s->x,s->y,140,92};}
static void render_shortcuts(void){for(uint32_t i=0;i<layout->shortcut_count&&i<OBS_LAYOUT_MAX_SHORTCUTS;i++){const obs_shortcut_config_t*s=&layout->shortcuts[i];const obs_app_metadata_t*app=obs_app_metadata_find(s->application_path);if(!app)continue;obs_work_area_t r;shortcut_rect(i,&r);int target=TARGET_SHORTCUT_BASE-(int)i;if(hover_target==target||pressed_target==target)obs_fill_rect(&canvas,r.x,r.y,(int32_t)r.width,(int32_t)r.height,pressed_target==target?theme->colors.panel_item_pressed:theme->colors.selection_background);int32_t icon_x=r.x+(int32_t)(r.width-theme->metrics.icon_size)/2;obs_draw_icon(&canvas,icon_x,r.y+8,theme->metrics.icon_size,app->icon,theme->colors.text_primary,theme->colors.accent_primary);uint32_t label_width=obs_text_width(app->display_name,theme->metrics.text_scale);int32_t label_x=r.x+(int32_t)(r.width-(label_width<r.width?label_width:r.width))/2;obs_draw_text_clipped(&canvas,label_x,r.y+60,r.width,app->display_name,theme->colors.desktop_foreground,theme->metrics.text_scale);}}
static void paint(void){draw_background();render_shortcuts();for(uint32_t i=0;i<layout->panel_count;i++)render_panel(i);}
static int contains(const obs_work_area_t*r,int32_t x,int32_t y){return x>=r->x&&y>=r->y&&x<r->x+(int32_t)r->width&&y<r->y+(int32_t)r->height;}
static int target_at(int32_t x,int32_t y){
    for(uint32_t p=0;p<layout->panel_count;p++){obs_work_area_t g;if(panel_geometry(p,&g)<0||!contains(&g,x,y))continue;const obs_panel_config_t*panel=&layout->panels[p];for(uint32_t m=0;m<panel->module_count;m++)if(panel->modules[m].type==OBS_PANEL_MODULE_LAUNCHER){obs_work_area_t r={g.x+module_origin_x(panel,m),g.y+(int32_t)(g.height-theme->metrics.task_entry_height)/2,panel->modules[m].extent-theme->metrics.spacing_medium,theme->metrics.task_entry_height};if(contains(&r,x,y))return TARGET_LAUNCHER;}for(uint32_t i=0;i<MAX_TASKS;i++)if(tasks[i].used){obs_work_area_t r;task_rect(&g,panel,i,&r);if(contains(&r,x,y))return(int)i;}return TARGET_NONE;}
    for(uint32_t i=0;i<layout->shortcut_count&&i<OBS_LAYOUT_MAX_SHORTCUTS;i++){obs_work_area_t r;shortcut_rect(i,&r);if(contains(&r,x,y))return TARGET_SHORTCUT_BASE-(int)i;}return TARGET_NONE;
}
static void launch_app(const obs_app_metadata_t*app){if(!app)return;int grant=(app->capabilities&OBS_APP_CAP_SETTINGS_WRITE)&&settings_authority>=0;if(grant)os_handle_set_inheritable((uint64_t)settings_authority,1);int64_t pid=obs_app_launch(app->path);if(grant)os_handle_set_inheritable((uint64_t)settings_authority,0);if(pid>=0){static const char message[]="desktop: application launched from shell module\n";sys_fd_write(1,message,sizeof(message)-1);}}
static void activate(int target,obs_window_t*desktop){if(target>=0&&target<MAX_TASKS&&tasks[target].used)obs_desktop_focus_restore(desktop,tasks[target].id);else if(target==TARGET_LAUNCHER){const obs_app_metadata_t*app=layout->shortcut_count?obs_app_metadata_find(layout->shortcuts[0].application_path):0;launch_app(app);}else if(target<=TARGET_SHORTCUT_BASE){uint32_t index=(uint32_t)(TARGET_SHORTCUT_BASE-target);if(index<layout->shortcut_count)launch_app(obs_app_metadata_find(layout->shortcuts[index].application_path));}}
static void management(const obs_desktop_management_event_t*event){
    if(event->type==OBS_DESKTOP_EVENT_WINDOW_ADDED){for(uint32_t i=0;i<MAX_TASKS;i++)if(!tasks[i].used){tasks[i]=(task_entry_t){1,0,event->window_id,event->state,{0}};copy_title(tasks[i].title,event->title);break;}}
    else if(event->type==OBS_DESKTOP_EVENT_WINDOW_REMOVED){for(uint32_t i=0;i<MAX_TASKS;i++)if(tasks[i].used&&tasks[i].id==event->window_id)tasks[i]=(task_entry_t){0};}
    else if(event->type==OBS_DESKTOP_EVENT_WINDOW_STATE){for(uint32_t i=0;i<MAX_TASKS;i++)if(tasks[i].used&&tasks[i].id==event->window_id)tasks[i].state=event->state;}
    else if(event->type==OBS_DESKTOP_EVENT_WINDOW_FOCUSED){for(uint32_t i=0;i<MAX_TASKS;i++)if(tasks[i].used)tasks[i].focused=tasks[i].id==event->window_id;}
}
int obsidia_main(void){
    active_theme=*obs_theme_get_default();active_layout=*obs_desktop_layout_default();theme=&active_theme;layout=&active_layout;settings_authority=os_handle_find(OS_OBJECT_IPC);if(settings_authority>=0)os_handle_set_inheritable((uint64_t)settings_authority,0);if(settings_authority>=0)obs_settings_connect_with_authority(&settings,(uint64_t)settings_authority);else obs_settings_connect(&settings);obs_setting_value_t value;if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_THEME,&value)==0)theme_id=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_TEXT_SCALE,&value)==0)text_scale=value.u32;active_theme=*obs_theme_get((obs_theme_id_t)theme_id);active_theme.metrics.text_scale=text_scale;if(obs_settings_get(&settings,OBS_SETTING_DESKTOP_BACKGROUND_MODE,&value)==0)active_layout.background_mode=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_PANEL_PRIMARY_EDGE,&value)==0)active_layout.panels[0].edge=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_PANEL_PRIMARY_SIZE,&value)==0)active_layout.panels[0].size=value.u32;obs_settings_subscribe(&settings,OBS_SETTINGS_SUBSCRIBE_ALL);
    fb_info_t info;sys_fbinfo(&info);if(!info.width||!info.height)return 1;obs_window_t desktop;if(obs_desktop_create_root(&desktop,(uint32_t)info.width,(uint32_t)info.height)<0)return 2;obs_canvas_init(&canvas,desktop.pixels,desktop.width,desktop.height,desktop.width);obs_work_area_t work;if(obs_desktop_work_area(layout,desktop.width,desktop.height,&work)<0||obs_desktop_configure_work_area(&desktop,work.x,work.y,work.width,work.height)<0)return 3;paint();if(os_window_present(&desktop)<0)return 4;static const char ready[]="desktop: live settings layout and panel modules ready\n";sys_fd_write(1,ready,sizeof(ready)-1);if(layout->shortcut_count)launch_app(obs_app_metadata_find(layout->shortcuts[0].application_path));
    for(;;){int processed=0,redraw=0,work_changed=0;obs_settings_event_t changed;for(uint32_t i=0;i<16&&obs_settings_try_next(&settings,&changed)>0;i++){processed=redraw=1;if(changed.setting_id==OBS_SETTING_APPEARANCE_THEME){theme_id=changed.value.enumeration;active_theme=*obs_theme_get((obs_theme_id_t)theme_id);active_theme.metrics.text_scale=text_scale;}else if(changed.setting_id==OBS_SETTING_APPEARANCE_TEXT_SCALE){text_scale=changed.value.u32;active_theme.metrics.text_scale=text_scale;}else if(changed.setting_id==OBS_SETTING_DESKTOP_BACKGROUND_MODE)active_layout.background_mode=changed.value.enumeration;else if(changed.setting_id==OBS_SETTING_PANEL_PRIMARY_EDGE){active_layout.panels[0].edge=changed.value.enumeration;work_changed=1;}else if(changed.setting_id==OBS_SETTING_PANEL_PRIMARY_SIZE){active_layout.panels[0].size=changed.value.u32;work_changed=1;}}
        for(uint32_t burst=0;burst<64;burst++){obs_window_event_t event={0};obs_desktop_management_event_t managed={0};int kind=obs_desktop_try_next_event(&desktop,&event,&managed);if(kind<=0)break;processed=1;if(kind==OBS_DESKTOP_NEXT_MANAGEMENT){management(&managed);redraw=1;}else if(event.type==OBS_WINDOW_EVENT_MOUSE_MOVE){int next=target_at(event.x,event.y);if(next!=hover_target){hover_target=next;redraw=1;}}else if(event.type==OBS_WINDOW_EVENT_MOUSE_BUTTON&&event.code==OBS_INPUT_BUTTON_LEFT){if(event.value){pressed_target=target_at(event.x,event.y);redraw=1;}else{int released=target_at(event.x,event.y),before=pressed_target;pressed_target=TARGET_NONE;if(before!=TARGET_NONE&&before==released)activate(before,&desktop);redraw=1;}}}
        if(work_changed&&obs_desktop_work_area(layout,desktop.width,desktop.height,&work)==0)
            obs_desktop_configure_work_area(&desktop,work.x,work.y,work.width,work.height);
        if(redraw){paint();os_window_present(&desktop);}
        if(processed)sys_yield();else sys_sleep(1);
    }
}
