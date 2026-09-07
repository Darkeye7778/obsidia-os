#pragma once
#include <stdint.h>
typedef enum {OBS_WINDOW_EVENT_KEY=1,OBS_WINDOW_EVENT_FOCUS=2,OBS_WINDOW_EVENT_MOUSE_BUTTON=3,OBS_WINDOW_EVENT_CLOSE=4,OBS_WINDOW_EVENT_RESIZE=5,OBS_WINDOW_EVENT_MOUSE_MOVE=6} obs_window_event_type_t;
typedef enum {OBS_WINDOW_NORMAL=0,OBS_WINDOW_MINIMIZED=1,OBS_WINDOW_MAXIMIZED=2} obs_window_state_t;
/* RESIZE uses code=transaction, value=state, and width/height. */
/* Compact high-rate application event ABI. Desktop metadata uses its own
 * bounded management event in obsidia/internal/desktop.h. */
typedef struct {uint32_t type,code;int32_t value;union{struct{int32_t x,y;};struct{uint32_t width,height;};};uint32_t reserved;uint64_t ticks;} obs_window_event_t;
typedef struct {uint64_t display_handle,session_handle,surface_handle;uint32_t*pixels;uint64_t mapping_address;uint32_t id,width,height,state;} obs_window_t;
int os_window_create(obs_window_t*window,uint32_t width,uint32_t height,const char*title);
uint32_t*os_window_pixels(obs_window_t*window);
int os_window_present(obs_window_t*window);
int os_window_next_event(obs_window_t*window,obs_window_event_t*event);
int os_window_try_next_event(obs_window_t*window,obs_window_event_t*event);
int os_window_close(obs_window_t*window);
int os_window_resize(obs_window_t*window,uint32_t width,uint32_t height);
int os_window_minimize(obs_window_t*window);
int os_window_maximize(obs_window_t*window);
int os_window_restore(obs_window_t*window);
