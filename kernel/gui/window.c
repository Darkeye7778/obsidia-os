#include "window.h"
#include "../console/console.h"
#include "../memory/heap.h"
#include "../memory/memory.h"
#include "../paging.h"
#include <stdint.h>

void serial_write(const char *str);

static void serial_print_hex64(uint64_t v) {
    char buf[20];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        int d = (v >> ((15 - i) * 4)) & 0xF;
        buf[2 + i] = (d < 10) ? ('0' + d) : ('A' + (d - 10));
    }
    buf[18] = 0;
    serial_write(buf);
}

static void serial_print_u32_hex(uint32_t v) {
    char buf[12];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        int d = (v >> ((7 - i) * 4)) & 0xF;
        buf[2 + i] = (d < 10) ? ('0' + d) : ('A' + (d - 10));
    }
    buf[10] = 0;
    serial_write(buf);
}

static window_t* window_list = 0;  // simple list, head is topmost for simplicity
static uint32_t next_win_id = 1;
static window_t* focused_window = 0;

void gui_window_init(void) {
    window_list = 0;
    focused_window = 0;
    console_print("GUI Window system initialized\n");
}

window_t* gui_window_create(int x, int y, uint64_t w, uint64_t h, const char* title) {
    // kmalloc for win descriptor (uses early arena pages which are mapped and written reliably)
    // surf uses pmm (its page has been observed to retain writes)
    window_t* win = (window_t*)kmalloc(sizeof(window_t));
    if (!win) {
        serial_write("WINDOW: kmalloc for win struct failed\n");
        return 0;
    }
    serial_write("WINDOW: win@");
    serial_print_hex64((uint64_t)win);
    serial_write("\n");
    // zero it 
    for (uint64_t i=0; i < sizeof(window_t); i++) ((uint8_t*)win)[i] = 0;

    win->surface = gui_surface_create(w, h);
    serial_write("WINDOW: after assign surf, readback=");
    serial_print_hex64((uint64_t)win->surface);
    serial_write("\n");
    if (!win->surface) {
        kfree(win);
        return 0;
    }
    serial_write("WINDOW: after surface check\n");

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
    serial_write("WINDOW: after raise\n");

    serial_write("WINDOW: create success\n");
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
    // pmm_free_page(win); // optional reclaim of descriptor page

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
        // Note: do not call raise here to avoid infinite recursion with raise->set_focus.
        // Raise (z-order) caller will ensure top position if needed.
    }
}

void gui_window_set_visible(window_t* win, int visible) {
    if (win) win->visible = visible;
}

window_t* gui_window_get_focused(void) {
    serial_write("WINDOW: get_focused ptr=");
    serial_print_hex64((uint64_t)focused_window);
    serial_write("\n");
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

void gui_window_add_to_list(window_t* win) {
    if (!win) return;
    win->next = window_list;
    window_list = win;
}

void gui_window_set_focused(window_t* win) {
    focused_window = win;
    if (win) {
        win->focused = 1;
    }
}
