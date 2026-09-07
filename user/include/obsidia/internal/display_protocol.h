#pragma once
#include <stdint.h>
#include <obsidia/input.h>

#define OBS_DISPLAY_CREATE_WINDOW 1U
#define OBS_DISPLAY_PRESENT_WINDOW 2U
#define OBS_DISPLAY_FORWARD_INPUT 3U
#define OBS_DISPLAY_CREATE_ROOT 4U
#define OBS_DISPLAY_CLOSE_WINDOW 5U
#define OBS_DISPLAY_QUERY_STATS 6U
#define OBS_DISPLAY_RESIZE_WINDOW 7U
#define OBS_DISPLAY_RESIZE_REPLY 8U
#define OBS_DISPLAY_WINDOW_ACTION 9U
#define OBS_DISPLAY_CONFIGURE_ROOT 10U
#define OBS_DISPLAY_MANAGE_WINDOW 11U

#define OBS_DISPLAY_ACTION_MINIMIZE 1U
#define OBS_DISPLAY_ACTION_MAXIMIZE 2U
#define OBS_DISPLAY_ACTION_RESTORE 3U
#define OBS_DISPLAY_MANAGE_FOCUS_RESTORE 1U
#define OBS_DISPLAY_RESIZE_ACCEPT 1U
#define OBS_DISPLAY_RESIZE_ABORT 2U

typedef struct { uint32_t operation,width,height,flags; char title[32]; } obs_display_create_request_t;
typedef struct { uint32_t operation,window_id; } obs_display_window_request_t;
typedef struct { uint32_t operation,window_id,width,height; } obs_display_resize_request_t;
typedef struct { uint32_t operation,window_id,transaction,result; } obs_display_resize_reply_t;
typedef struct { uint32_t operation,window_id,action,reserved; } obs_display_action_request_t;
typedef struct { uint32_t operation,root_id; int32_t x,y; uint32_t width,height; } obs_display_root_config_request_t;
typedef struct { uint32_t operation,root_id,window_id,action; } obs_display_manage_request_t;
typedef struct { uint32_t operation; obs_input_event_t event; } obs_display_input_request_t;
typedef struct { int32_t status; uint32_t window_id,width,height; } obs_display_create_response_t;
typedef struct { int32_t status; } obs_display_close_response_t;
typedef struct { uint32_t operation,reserved; } obs_display_query_request_t;
typedef struct { int32_t status; uint32_t live_windows; uint64_t damage_events,full_damage_fallbacks,rejected_requests,resize_commits,resize_aborts; int32_t cursor_x,cursor_y; uint64_t input_events; } obs_display_stats_response_t;
