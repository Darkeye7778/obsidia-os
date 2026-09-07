#pragma once
#include <stdint.h>

typedef struct {
    uint32_t desktop_background,desktop_background_secondary,desktop_foreground;
    uint32_t panel_background,panel_border,panel_item,panel_item_hover,panel_item_pressed;
    uint32_t window_background,window_border_active,window_border_inactive,window_shadow;
    uint32_t titlebar_active,titlebar_inactive,title_text_active,title_text_inactive;
    uint32_t control_normal,control_hover,control_pressed,control_danger,control_danger_hover;
    uint32_t text_primary,text_secondary,text_disabled;
    uint32_t accent_primary,accent_secondary;
    uint32_t selection_background,selection_foreground;
} obs_theme_colors_t;

typedef struct {
    uint32_t border_width,titlebar_height,control_size,control_gap,control_margin;
    uint32_t spacing_small,spacing_medium,spacing_large;
    uint32_t icon_size,text_scale,shadow_size,task_entry_width,task_entry_height;
} obs_theme_metrics_t;

typedef struct { const char *name; obs_theme_colors_t colors; obs_theme_metrics_t metrics; } obs_theme_t;
typedef enum { OBS_THEME_DEFAULT=0,OBS_THEME_ALTERNATE=1 } obs_theme_id_t;

const obs_theme_t *obs_theme_get(obs_theme_id_t id);
const obs_theme_t *obs_theme_get_default(void);
