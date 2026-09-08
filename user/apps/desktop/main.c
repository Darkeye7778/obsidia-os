#include "syscall.h"
#include <obsidia/app.h>
#include <obsidia/app_catalog.h>
#include <obsidia/app_metadata.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/gfx.h>
#include <obsidia/input.h>
#include <obsidia/theme.h>
#include <obsidia/settings.h>
#include <obsidia/window.h>
#include <obsidia/internal/desktop.h>
#include <obsidia/internal/shell_model.h>

#define TARGET_NONE (-1)
#define TARGET_LAUNCHER (-2)
#define TARGET_SHORTCUT_BASE (-100)
#define TARGET_LAUNCHER_APP_BASE 1000
#define TARGET_LAUNCHER_PIN_BASE 2000
#define TARGET_LAUNCHER_PREV 3000
#define TARGET_LAUNCHER_NEXT 3001
#define LAUNCHER_WIDTH 340U
#define LAUNCHER_ROW_HEIGHT 56U
#define LAUNCHER_HEADER_HEIGHT 44U
#define LAUNCHER_MAX_ROWS 8U

static const obs_theme_t*theme;
static const obs_desktop_layout_t*layout;
static obs_theme_t active_theme;
static obs_desktop_layout_t active_layout;
static obs_settings_t settings;
static obs_app_catalog_t catalog;
static obs_pinned_apps_t pins;
static int64_t settings_authority=-1;
static uint32_t theme_id=OBS_THEME_DEFAULT,text_scale=2;
static obs_canvas_t canvas,overlay_canvas;
static obs_shell_model_t shell;
static obs_shell_overlay_t overlay;
static int launcher_open,overlay_x,overlay_y;
static uint32_t launcher_first,launcher_rows;
static int hover_target=TARGET_NONE,pressed_target=TARGET_NONE;
static uint8_t capacity_logged;

static void log(const char*s){uint32_t n=0;while(s[n])n++;sys_fd_write(1,s,n);}
static int contains(const obs_work_area_t*r,int32_t x,int32_t y){return x>=r->x&&y>=r->y&&x<r->x+(int32_t)r->width&&y<r->y+(int32_t)r->height;}
static void draw_background(void){
    obs_fill_rect(&canvas,0,0,(int32_t)canvas.width,(int32_t)canvas.height,theme->colors.desktop_background);
    if(layout->background_mode==OBS_BACKGROUND_SUBTLE_BANDS){uint32_t band=canvas.height/5;if(!band)band=1;for(uint32_t i=0;i<5;i++)if(i&1)obs_fill_rect(&canvas,0,(int32_t)(i*band),(int32_t)canvas.width,(int32_t)band,theme->colors.desktop_background_secondary);obs_fill_rect(&canvas,(int32_t)(canvas.width*2/3),0,(int32_t)(canvas.width/3),(int32_t)canvas.height,theme->colors.desktop_background);}
}
static int panel_geometry(uint32_t index,obs_work_area_t*g){return index<layout->panel_count?obs_panel_geometry(&layout->panels[index],canvas.width,canvas.height,g):-1;}
static int32_t module_origin_x(const obs_panel_config_t*panel,uint32_t module){int32_t x=(int32_t)theme->metrics.spacing_medium;for(uint32_t i=0;i<module;i++)x+=(int32_t)panel->modules[i].extent;return x;}
static void launcher_rect(const obs_work_area_t*g,const obs_panel_config_t*panel,uint32_t module,obs_work_area_t*r){*r=(obs_work_area_t){g->x+module_origin_x(panel,module),g->y+(int32_t)(g->height-theme->metrics.task_entry_height)/2,panel->modules[module].extent-theme->metrics.spacing_medium,theme->metrics.task_entry_height};}
static void render_launcher(const obs_work_area_t*g,const obs_panel_config_t*panel,uint32_t module){
    obs_work_area_t r;launcher_rect(g,panel,module,&r);uint32_t color=pressed_target==TARGET_LAUNCHER?theme->colors.panel_item_pressed:(hover_target==TARGET_LAUNCHER||launcher_open?theme->colors.panel_item_hover:theme->colors.panel_item);
    obs_fill_rect(&canvas,r.x,r.y,(int32_t)r.width,(int32_t)r.height,color);obs_draw_icon(&canvas,r.x+6,r.y+4,24,OBS_ICON_SYSTEM,theme->colors.text_primary,theme->colors.accent_primary);obs_draw_text_clipped(&canvas,r.x+38,r.y+9,r.width>44?r.width-44:0,"Obsidia",theme->colors.text_primary,theme->metrics.text_scale);
}
static int32_t groups_origin(const obs_panel_config_t*panel){for(uint32_t i=0;i<panel->module_count;i++)if(panel->modules[i].type==OBS_PANEL_MODULE_RUNNING_APPS)return module_origin_x(panel,i);return 0;}
static void group_rect(const obs_work_area_t*g,const obs_panel_config_t*panel,uint32_t visible,obs_work_area_t*r){uint32_t h=theme->metrics.task_entry_height,w=theme->metrics.task_entry_width;r->x=g->x+groups_origin(panel)+(int32_t)(visible*(w+theme->metrics.spacing_small));r->y=g->y+(int32_t)(g->height-h)/2;r->width=w;r->height=h;}
static int group_focused(const obs_shell_app_group_t*g){for(uint32_t i=0;i<OBS_SHELL_MAX_WINDOWS_PER_GROUP;i++)if(g->windows[i].used&&g->windows[i].focused)return 1;return 0;}
static void render_groups(const obs_work_area_t*g,const obs_panel_config_t*panel){
    uint32_t count=obs_shell_group_count(&shell);for(uint32_t visible=0;visible<count;visible++){obs_shell_app_group_t*group=obs_shell_group_at(&shell,visible);if(!group)continue;obs_work_area_t r;group_rect(g,panel,visible,&r);if(r.x>=(int32_t)(g->x+g->width))continue;
        int focused=group_focused(group),all_minimized=obs_shell_group_all_minimized(group);uint32_t color=pressed_target==(int)visible?theme->colors.panel_item_pressed:(hover_target==(int)visible?theme->colors.panel_item_hover:theme->colors.panel_item);if(all_minimized)color=theme->colors.titlebar_inactive;
        obs_fill_rect(&canvas,r.x,r.y,(int32_t)r.width,(int32_t)r.height,color);if(focused)obs_stroke_rect(&canvas,r.x,r.y,(int32_t)r.width,(int32_t)r.height,2,theme->colors.accent_primary);else obs_hline(&canvas,r.x,r.y+(int32_t)r.height-1,(int32_t)r.width,theme->colors.panel_border);
        obs_draw_icon(&canvas,r.x+6,r.y+6,20,group->icon,all_minimized?theme->colors.text_disabled:theme->colors.text_secondary,theme->colors.accent_secondary);obs_draw_text_clipped(&canvas,r.x+32,r.y+9,r.width>52?r.width-52:0,group->display_name,all_minimized?theme->colors.text_disabled:theme->colors.text_primary,theme->metrics.text_scale);
        if(group->window_count){uint32_t line=focused?theme->colors.accent_primary:theme->colors.accent_secondary;obs_fill_rect(&canvas,r.x+6,r.y+(int32_t)r.height-3,(int32_t)r.width-12,2,line);}
        if(group->window_count>1){char number[3]={(char)('0'+group->window_count/10),(char)('0'+group->window_count%10),0};const char*text=group->window_count<10?number+1:number;obs_draw_text(&canvas,r.x+(int32_t)r.width-18,r.y+4,text,theme->colors.selection_foreground,1);}
    }
}
static void render_panel(uint32_t index){obs_work_area_t g;if(panel_geometry(index,&g)<0)return;const obs_panel_config_t*panel=&layout->panels[index];obs_fill_rect(&canvas,g.x,g.y,(int32_t)g.width,(int32_t)g.height,theme->colors.panel_background);if(panel->edge==OBS_PANEL_TOP)obs_hline(&canvas,g.x,g.y+(int32_t)g.height-1,(int32_t)g.width,theme->colors.panel_border);else if(panel->edge==OBS_PANEL_BOTTOM)obs_hline(&canvas,g.x,g.y,(int32_t)g.width,theme->colors.panel_border);for(uint32_t i=0;i<panel->module_count&&i<OBS_LAYOUT_MAX_MODULES;i++){if(panel->modules[i].type==OBS_PANEL_MODULE_LAUNCHER)render_launcher(&g,panel,i);else if(panel->modules[i].type==OBS_PANEL_MODULE_RUNNING_APPS)render_groups(&g,panel);}}
static void shortcut_rect(uint32_t index,obs_work_area_t*r){const obs_shortcut_config_t*s=&layout->shortcuts[index];*r=(obs_work_area_t){s->x,s->y,140,92};}
static void render_shortcuts(void){for(uint32_t i=0;i<layout->shortcut_count&&i<OBS_LAYOUT_MAX_SHORTCUTS;i++){const obs_shortcut_config_t*s=&layout->shortcuts[i];const obs_app_metadata_t*app=obs_app_catalog_find_path(&catalog,s->application_path);if(!app)continue;obs_work_area_t r;shortcut_rect(i,&r);int target=TARGET_SHORTCUT_BASE-(int)i;if(hover_target==target||pressed_target==target)obs_fill_rect(&canvas,r.x,r.y,(int32_t)r.width,(int32_t)r.height,pressed_target==target?theme->colors.panel_item_pressed:theme->colors.selection_background);int32_t icon_x=r.x+(int32_t)(r.width-theme->metrics.icon_size)/2;obs_draw_icon(&canvas,icon_x,r.y+8,theme->metrics.icon_size,app->icon,theme->colors.text_primary,theme->colors.accent_primary);uint32_t label_width=obs_text_width(app->display_name,theme->metrics.text_scale);int32_t label_x=r.x+(int32_t)(r.width-(label_width<r.width?label_width:r.width))/2;obs_draw_text_clipped(&canvas,label_x,r.y+60,r.width,app->display_name,theme->colors.desktop_foreground,theme->metrics.text_scale);}}
static void paint(void){draw_background();render_shortcuts();for(uint32_t i=0;i<layout->panel_count;i++)render_panel(i);}

static void overlay_position(void){if(!layout->panel_count||obs_panel_popup_position(&layout->panels[0],canvas.width,canvas.height,overlay.width,overlay.height,8,12,&overlay_x,&overlay_y)<0){overlay_x=12;overlay_y=12;}}
static int app_pinned(const char*id){for(uint32_t i=0;i<pins.count;i++){const char*a=pins.ids[i],*b=id;while(*a&&*a==*b){a++;b++;}if(!*a&&!*b)return 1;}return 0;}
static void paint_overlay(void){
    obs_fill_rect(&overlay_canvas,0,0,(int32_t)overlay.width,(int32_t)overlay.height,theme->colors.panel_background);obs_stroke_rect(&overlay_canvas,0,0,(int32_t)overlay.width,(int32_t)overlay.height,2,theme->colors.window_border_active);obs_draw_text(&overlay_canvas,16,14,"Applications",theme->colors.text_primary,theme->metrics.text_scale);
    uint32_t count=obs_app_catalog_count(&catalog);if(!count){obs_draw_text(&overlay_canvas,18,58,"No applications installed",theme->colors.text_secondary,theme->metrics.text_scale);return;}if(launcher_first>=count)launcher_first=0;
    uint32_t shown=count-launcher_first;if(shown>launcher_rows)shown=launcher_rows;for(uint32_t row=0;row<shown;row++){uint32_t i=launcher_first+row;const obs_app_metadata_t*app=obs_app_catalog_at(&catalog,i);int target=TARGET_LAUNCHER_APP_BASE+(int)i,pin_target=TARGET_LAUNCHER_PIN_BASE+(int)i;int32_t y=(int32_t)LAUNCHER_HEADER_HEIGHT+(int32_t)row*LAUNCHER_ROW_HEIGHT;uint32_t color=pressed_target==target?theme->colors.panel_item_pressed:(hover_target==target?theme->colors.panel_item_hover:theme->colors.panel_item);obs_fill_rect(&overlay_canvas,10,y,(int32_t)overlay.width-20,48,color);obs_draw_icon(&overlay_canvas,18,y+8,32,app->icon,theme->colors.text_secondary,theme->colors.accent_primary);obs_draw_text_clipped(&overlay_canvas,62,y+16,overlay.width-126,app->display_name,theme->colors.text_primary,theme->metrics.text_scale);uint32_t pc=pressed_target==pin_target?theme->colors.panel_item_pressed:(hover_target==pin_target?theme->colors.panel_item_hover:theme->colors.control_normal);obs_fill_rect(&overlay_canvas,(int32_t)overlay.width-50,y+7,32,32,pc);obs_stroke_rect(&overlay_canvas,(int32_t)overlay.width-50,y+7,32,32,1,app_pinned(app->id)?theme->colors.accent_primary:theme->colors.window_border_inactive);obs_draw_text(&overlay_canvas,(int32_t)overlay.width-39,y+15,app_pinned(app->id)?"-":"+",theme->colors.text_primary,1);}
    if(count>launcher_rows){int32_t y=(int32_t)overlay.height-28;obs_draw_text(&overlay_canvas,18,y+8,"Previous",theme->colors.text_secondary,1);obs_draw_text(&overlay_canvas,(int32_t)overlay.width-62,y+8,"Next",theme->colors.text_secondary,1);}
}
static void show_launcher(obs_window_t*desktop,int show){launcher_open=show?1:0;if(launcher_open)launcher_first=0;hover_target=pressed_target=TARGET_NONE;if(launcher_open){overlay_position();paint_overlay();obs_desktop_overlay_configure(desktop,&overlay,overlay_x,overlay_y,1);}else obs_desktop_overlay_configure(desktop,&overlay,overlay_x,overlay_y,0);paint();os_window_present(desktop);}
static int overlay_target(int32_t x,int32_t y){if(!launcher_open)return TARGET_NONE;int32_t lx=x-overlay_x,ly=y-overlay_y;if(lx<0||ly<0||lx>=(int32_t)overlay.width||ly>=(int32_t)overlay.height)return TARGET_NONE;uint32_t count=obs_app_catalog_count(&catalog),shown=count>launcher_first?count-launcher_first:0;if(shown>launcher_rows)shown=launcher_rows;for(uint32_t row=0;row<shown;row++){uint32_t i=launcher_first+row;obs_work_area_t pin={(int32_t)overlay.width-50,(int32_t)LAUNCHER_HEADER_HEIGHT+(int32_t)row*LAUNCHER_ROW_HEIGHT+7,32,32};if(contains(&pin,lx,ly))return TARGET_LAUNCHER_PIN_BASE+(int)i;obs_work_area_t r={10,(int32_t)LAUNCHER_HEADER_HEIGHT+(int32_t)row*LAUNCHER_ROW_HEIGHT,overlay.width-68,48};if(contains(&r,lx,ly))return TARGET_LAUNCHER_APP_BASE+(int)i;}if(count>launcher_rows&&ly>=(int32_t)overlay.height-28)return lx<(int32_t)overlay.width/2?TARGET_LAUNCHER_PREV:TARGET_LAUNCHER_NEXT;return TARGET_NONE;}
static int target_at(int32_t x,int32_t y){
    int overlay_hit=overlay_target(x,y);if(overlay_hit!=TARGET_NONE)return overlay_hit;
    for(uint32_t p=0;p<layout->panel_count;p++){obs_work_area_t g;if(panel_geometry(p,&g)<0||!contains(&g,x,y))continue;const obs_panel_config_t*panel=&layout->panels[p];for(uint32_t m=0;m<panel->module_count;m++)if(panel->modules[m].type==OBS_PANEL_MODULE_LAUNCHER){obs_work_area_t r;launcher_rect(&g,panel,m,&r);if(contains(&r,x,y))return TARGET_LAUNCHER;}uint32_t count=obs_shell_group_count(&shell);for(uint32_t i=0;i<count;i++){obs_work_area_t r;group_rect(&g,panel,i,&r);if(contains(&r,x,y))return(int)i;}return TARGET_NONE;}
    for(uint32_t i=0;i<layout->shortcut_count&&i<OBS_LAYOUT_MAX_SHORTCUTS;i++){obs_work_area_t r;shortcut_rect(i,&r);if(contains(&r,x,y))return TARGET_SHORTCUT_BASE-(int)i;}return TARGET_NONE;
}
static void launch_app(const obs_app_metadata_t*app){if(!app)return;int grant=(app->capabilities&OBS_APP_CAP_SETTINGS_WRITE)&&settings_authority>=0;if(grant)os_handle_set_inheritable((uint64_t)settings_authority,1);int64_t pid=obs_app_launch(app->path);if(grant)os_handle_set_inheritable((uint64_t)settings_authority,0);if(pid>=0)log("desktop: application launched from shell\n");}
static void reload_pins(void){obs_pinned_apps_t next;if(obs_settings_get_pinned_apps(&settings,&next)==0){pins=next;(void)obs_shell_sync_pins(&shell,&catalog,&pins);}}
static void activate_closed(int target,obs_window_t*desktop){
    if(target>=0&&target<(int)OBS_SHELL_MAX_APP_GROUPS){obs_shell_app_group_t*g=obs_shell_group_at(&shell,(uint32_t)target);if(!g)return;if(!g->window_count)launch_app(obs_app_catalog_find_id(&catalog,g->application_id));else{uint32_t id=obs_shell_group_choose_window(g);if(id)obs_desktop_focus_restore(desktop,id);}return;}
    if(target==TARGET_LAUNCHER){show_launcher(desktop,1);return;}
    if(target<=TARGET_SHORTCUT_BASE){uint32_t index=(uint32_t)(TARGET_SHORTCUT_BASE-target);if(index<layout->shortcut_count)launch_app(obs_app_catalog_find_path(&catalog,layout->shortcuts[index].application_path));}
}
static void activate_overlay(int target,obs_window_t*desktop){
    uint32_t count=obs_app_catalog_count(&catalog);
    if(target>=TARGET_LAUNCHER_APP_BASE&&target<TARGET_LAUNCHER_APP_BASE+(int)count){const obs_app_metadata_t*app=obs_app_catalog_at(&catalog,(uint32_t)(target-TARGET_LAUNCHER_APP_BASE));show_launcher(desktop,0);launch_app(app);return;}
    if(target>=TARGET_LAUNCHER_PIN_BASE&&target<TARGET_LAUNCHER_PIN_BASE+(int)count){const obs_app_metadata_t*app=obs_app_catalog_at(&catalog,(uint32_t)(target-TARGET_LAUNCHER_PIN_BASE));if(app){if(app_pinned(app->id))obs_settings_unpin_app(&settings,app->id);else obs_settings_pin_app(&settings,app->id);reload_pins();paint_overlay();paint();os_window_present(desktop);}return;}
    if(target==TARGET_LAUNCHER_PREV){launcher_first=launcher_first>launcher_rows?launcher_first-launcher_rows:0;paint_overlay();return;}
    if(target==TARGET_LAUNCHER_NEXT){if(launcher_first+launcher_rows<count)launcher_first+=launcher_rows;paint_overlay();return;}
    show_launcher(desktop,0);
}
static void management(const obs_desktop_management_event_t*event){
    int result=0;if(event->type==OBS_DESKTOP_EVENT_WINDOW_ADDED)result=obs_shell_window_added(&shell,&catalog,event->window_id,event->owner_pid,event->title,event->state);else if(event->type==OBS_DESKTOP_EVENT_WINDOW_REMOVED)result=obs_shell_window_removed(&shell,event->window_id);else if(event->type==OBS_DESKTOP_EVENT_WINDOW_STATE)result=obs_shell_window_state(&shell,event->window_id,event->state);else if(event->type==OBS_DESKTOP_EVENT_WINDOW_FOCUSED)result=obs_shell_window_focused(&shell,event->window_id);
    if(result<0&&event->type==OBS_DESKTOP_EVENT_WINDOW_ADDED&&!capacity_logged){capacity_logged=1;log("desktop: application-group capacity exhausted or identity unavailable\n");}
}

int obsidia_main(void){
    active_theme=*obs_theme_get_default();active_layout=*obs_desktop_layout_default();theme=&active_theme;layout=&active_layout;if(obs_app_catalog_connect(&catalog)<0)return 1;settings_authority=os_handle_find(OS_OBJECT_IPC);if(settings_authority>=0)os_handle_set_inheritable((uint64_t)settings_authority,0);if(settings_authority>=0)obs_settings_connect_with_authority(&settings,(uint64_t)settings_authority);else obs_settings_connect(&settings);obs_setting_value_t value;if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_THEME,&value)==0)theme_id=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_TEXT_SCALE,&value)==0)text_scale=value.u32;active_theme=*obs_theme_get((obs_theme_id_t)theme_id);active_theme.metrics.text_scale=text_scale;if(obs_settings_get(&settings,OBS_SETTING_DESKTOP_BACKGROUND_MODE,&value)==0)active_layout.background_mode=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_PANEL_PRIMARY_EDGE,&value)==0)active_layout.panels[0].edge=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_PANEL_PRIMARY_SIZE,&value)==0)active_layout.panels[0].size=value.u32;obs_settings_subscribe(&settings,OBS_SETTINGS_SUBSCRIBE_ALL);if(obs_settings_get_pinned_apps(&settings,&pins)<0)pins.count=0;obs_shell_model_init(&shell,&catalog,&pins);
    fb_info_t info;sys_fbinfo(&info);if(!info.width||!info.height)return 2;obs_window_t desktop;if(obs_desktop_create_root(&desktop,(uint32_t)info.width,(uint32_t)info.height)<0)return 3;obs_canvas_init(&canvas,desktop.pixels,desktop.width,desktop.height,desktop.width);uint32_t app_count=obs_app_catalog_count(&catalog),launcher_height=0;if(obs_launcher_overlay_layout(app_count,desktop.height,&launcher_height,&launcher_rows)<0)return 4;if(obs_desktop_overlay_create(&desktop,&overlay,LAUNCHER_WIDTH,launcher_height)<0)return 4;obs_canvas_init(&overlay_canvas,overlay.pixels,overlay.width,overlay.height,overlay.width);obs_work_area_t work;if(obs_desktop_work_area(layout,desktop.width,desktop.height,&work)<0||obs_desktop_configure_work_area(&desktop,work.x,work.y,work.width,work.height)<0)return 5;paint();if(os_window_present(&desktop)<0)return 6;log("desktop: live catalog shell ready (no startup apps)\n");
    for(;;){int processed=0,redraw=0,work_changed=0;obs_settings_event_t changed;for(uint32_t i=0;i<16&&obs_settings_try_next(&settings,&changed)>0;i++){processed=redraw=1;if(changed.setting_id==OBS_SETTING_APPEARANCE_THEME){theme_id=changed.value.enumeration;active_theme=*obs_theme_get((obs_theme_id_t)theme_id);active_theme.metrics.text_scale=text_scale;}else if(changed.setting_id==OBS_SETTING_APPEARANCE_TEXT_SCALE){text_scale=changed.value.u32;active_theme.metrics.text_scale=text_scale;}else if(changed.setting_id==OBS_SETTING_DESKTOP_BACKGROUND_MODE)active_layout.background_mode=changed.value.enumeration;else if(changed.setting_id==OBS_SETTING_PANEL_PRIMARY_EDGE){active_layout.panels[0].edge=changed.value.enumeration;work_changed=1;}else if(changed.setting_id==OBS_SETTING_PANEL_PRIMARY_SIZE){active_layout.panels[0].size=changed.value.u32;work_changed=1;}else if(changed.setting_id==OBS_SETTING_SHELL_PINNED_APPS)reload_pins();}
        for(uint32_t burst=0;burst<64;burst++){obs_window_event_t event={0};obs_desktop_management_event_t managed={0};int kind=obs_desktop_try_next_event(&desktop,&event,&managed);if(kind<=0)break;processed=1;if(kind==OBS_DESKTOP_NEXT_MANAGEMENT){management(&managed);redraw=1;}else if(event.type==OBS_WINDOW_EVENT_MOUSE_MOVE){int next=target_at(event.x,event.y);if(next!=hover_target){hover_target=next;redraw=1;if(launcher_open)paint_overlay();}}else if(event.type==OBS_WINDOW_EVENT_MOUSE_BUTTON&&event.code==OBS_INPUT_BUTTON_LEFT){if(event.value){pressed_target=target_at(event.x,event.y);redraw=1;if(launcher_open)paint_overlay();}else{int released=target_at(event.x,event.y),before=pressed_target;pressed_target=TARGET_NONE;if(launcher_open){if(before==released)activate_overlay(released,&desktop);else show_launcher(&desktop,0);}else if(before!=TARGET_NONE&&before==released)activate_closed(before,&desktop);redraw=1;}}}
        if(work_changed&&obs_desktop_work_area(layout,desktop.width,desktop.height,&work)==0){obs_desktop_configure_work_area(&desktop,work.x,work.y,work.width,work.height);if(launcher_open){overlay_position();paint_overlay();obs_desktop_overlay_configure(&desktop,&overlay,overlay_x,overlay_y,1);}}
        if(redraw){paint();if(launcher_open)paint_overlay();os_window_present(&desktop);}
        if(processed)sys_yield();else sys_sleep(1);
    }
}
