#pragma once
#include <stdint.h>
#include <obsidia/window.h>
#define OBS_DESKTOP_EVENT_WINDOW_ADDED 0x1001U
#define OBS_DESKTOP_EVENT_WINDOW_REMOVED 0x1002U
#define OBS_DESKTOP_EVENT_WINDOW_STATE 0x1003U
#define OBS_DESKTOP_EVENT_WINDOW_FOCUSED 0x1004U
typedef struct {
    uint32_t type,window_id,state,reserved;
    uint64_t ticks;
    char title[32];
    uint64_t reserved2;
} obs_desktop_management_event_t;
#define OBS_DESKTOP_NEXT_APPLICATION 1
#define OBS_DESKTOP_NEXT_MANAGEMENT 2
int obs_desktop_create_root(obs_window_t*window,uint32_t width,uint32_t height);
int obs_desktop_configure_work_area(obs_window_t*window,int32_t x,int32_t y,uint32_t width,uint32_t height);
int obs_desktop_focus_restore(obs_window_t*window,uint32_t window_id);
int obs_desktop_next_event(obs_window_t*window,obs_window_event_t*application,obs_desktop_management_event_t*management);
int obs_desktop_try_next_event(obs_window_t*window,obs_window_event_t*application,obs_desktop_management_event_t*management);
