#pragma once
#include <stdint.h>
#include "surface.h"

typedef struct window {
    uint32_t id;
    int x, y;
    uint64_t width, height;
    int z;                  // higher = on top
    int focused;
    int visible;
    surface_t* surface;     // backing surface
    char title[64];
    struct window* next;
} window_t;

void gui_window_init(void);

window_t* gui_window_create(int x, int y, uint64_t w, uint64_t h, const char* title);
void gui_window_destroy(window_t* win);

void gui_window_move(window_t* win, int dx, int dy);
void gui_window_set_focus(window_t* win);
void gui_window_set_visible(window_t* win, int visible);

window_t* gui_window_get_focused(void);
window_t* gui_window_find_at(int x, int y);

// For compositor
window_t* gui_window_get_list(void);  // head of list (Z order managed by list)
void gui_window_raise(window_t* win); // bring to front

// Helpers for manual window setup (bypass create if needed)
void gui_window_add_to_list(window_t* win);
void gui_window_set_focused(window_t* win);
