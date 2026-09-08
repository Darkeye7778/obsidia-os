#pragma once
#include <stdint.h>

#define OBS_LAYOUT_MAX_PANELS 2U
#define OBS_LAYOUT_MAX_MODULES 4U
#define OBS_LAYOUT_MAX_SHORTCUTS 8U

typedef enum {OBS_PANEL_TOP=1,OBS_PANEL_BOTTOM=2,OBS_PANEL_LEFT=3,OBS_PANEL_RIGHT=4} obs_panel_edge_t;
typedef enum {OBS_PANEL_MODULE_LAUNCHER=1,OBS_PANEL_MODULE_RUNNING_APPS=2,OBS_PANEL_MODULE_SPACER=3} obs_panel_module_type_t;
typedef enum {OBS_BACKGROUND_SOLID=1,OBS_BACKGROUND_SUBTLE_BANDS=2} obs_background_mode_t;

typedef struct {uint32_t type;uint32_t extent;} obs_panel_module_config_t;
typedef struct {uint32_t edge,size,module_count;obs_panel_module_config_t modules[OBS_LAYOUT_MAX_MODULES];} obs_panel_config_t;
typedef struct {const char *application_path;int32_t x,y;} obs_shortcut_config_t;
typedef struct {
    uint32_t background_mode,panel_count,shortcut_count;
    obs_panel_config_t panels[OBS_LAYOUT_MAX_PANELS];
    obs_shortcut_config_t shortcuts[OBS_LAYOUT_MAX_SHORTCUTS];
} obs_desktop_layout_t;
typedef struct {int32_t x,y;uint32_t width,height;} obs_work_area_t;

const obs_desktop_layout_t *obs_desktop_layout_default(void);
int obs_desktop_work_area(const obs_desktop_layout_t *layout,uint32_t screen_width,uint32_t screen_height,obs_work_area_t *area);
int obs_panel_geometry(const obs_panel_config_t *panel,uint32_t screen_width,uint32_t screen_height,obs_work_area_t *geometry);
int obs_panel_popup_position(const obs_panel_config_t *panel,uint32_t screen_width,uint32_t screen_height,uint32_t popup_width,uint32_t popup_height,uint32_t gap,uint32_t edge_offset,int32_t *x,int32_t *y);
int obs_launcher_overlay_layout(uint32_t application_count,uint32_t screen_height,uint32_t *height,uint32_t *visible_rows);
