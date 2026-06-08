#include "window.h"
#include "../console/console.h"
#include "../memory/heap.h"
#include <stdint.h>

static window_t* window_list = 0;  // simple list, head is topmost for simplicity
static uint32_t next_win_id = 1;
static window_t* focused_window = 0;

void gui_window_init(void) {
    window_list = 0;
    focused_window = 0;
    console_print("GUI Window system initialized\n");
}

window_t* gui_window_create(int x, int y, uint64_t w, uint64_t h, const char* title) {
    window_t* win = (window_t*)kmalloc(sizeof(window_t));
    if (!win) return 0;

    win->surface = gui_surface_create(w, h);
    if (!win->surface) {
        // kfree(win);
        return 0;
    }

    win->id = next_win_id++;
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->z = 0; // will be raised
    win->focused = 0;
    win->visible = 1;

    int i = 0;
    if (title) {
        for (; title[i] && i < 63; i++) win->title[i] = title[i];
    }
    win->title[i] = 0;

    // Insert at head (top)
    win->next = window_list;
    window_list = win;

    gui_window_raise(win); // ensure on top and focus

    console_print("Window created: ");
    console_print(win->title);
    console_print("\n");

    return win;
}

void gui_window_destroy(window_t* win) {
    if (!win) return;

    // Remove from list
    window_t** pp = &window_list;
    while (*pp) {
        if (*pp == win) {
            *pp = win->next;
            break;
        }
        pp = &(*pp)->next;
    }

    if (focused_window == win) focused_window = 0;

    if (win->surface) gui_surface_destroy(win->surface);
    // kfree(win);

    console_print("Window destroyed\n");
}

void gui_window_move(window_t* win, int dx, int dy) {
    if (!win) return;
    win->x += dx;
    win->y += dy;
    if (win->surface) gui_surface_mark_dirty(win->surface);
}

void gui_window_set_focus(window_t* win) {
    if (focused_window) focused_window->focused = 0;
    focused_window = win;
    if (win) {
        win->focused = 1;
        gui_window_raise(win);
    }
}

void gui_window_set_visible(window_t* win, int visible) {
    if (win) win->visible = visible;
}

window_t* gui_window_get_focused(void) {
    return focused_window;
}

window_t* gui_window_find_at(int x, int y) {
    window_t* w = window_list;
    while (w) {
        if (w->visible &&
            x >= w->x && x < w->x + (int)w->width &&
            y >= w->y && y < w->y + (int)w->height) {
            return w;
        }
        w = w->next;
    }
    return 0;
}

window_t* gui_window_get_list(void) {
    return window_list;
}

void gui_window_raise(window_t* win) {
    if (!win) return;

    // Remove from current position
    window_t** pp = &window_list;
    while (*pp && *pp != win) pp = &(*pp)->next;
    if (*pp) *pp = win->next;

    // Insert at head (topmost)
    win->next = window_list;
    window_list = win;

    gui_window_set_focus(win);
}
