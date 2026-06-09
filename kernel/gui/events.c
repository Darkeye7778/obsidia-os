#include "events.h"
#include "window.h"
#include "../console/console.h"
#include <stdint.h>

void serial_write(const char *str);

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

#define EVENT_QUEUE_SIZE 64

static gui_event_t event_queue[EVENT_QUEUE_SIZE];
static int event_head = 0;
static int event_tail = 0;

void gui_events_init(void) {
    event_head = event_tail = 0;
    console_print("GUI Event system initialized\n");
}

void gui_post_event(gui_event_t* ev) {
    if (!ev) return;
    int next = (event_head + 1) % EVENT_QUEUE_SIZE;
    if (next == event_tail) {
        // queue full, drop oldest
        event_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
    }
    event_queue[event_head] = *ev;
    event_head = next;
}

int gui_get_event(gui_event_t* out) {
    if (event_tail == event_head) return 0; // empty
    if (out) *out = event_queue[event_tail];
    event_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
    return 1;
}

void gui_process_events(void) {
    serial_write("EVENTS: process_events got event\n");
    gui_event_t ev;
    while (gui_get_event(&ev)) {
        serial_write("EVENTS: processing key=");
        serial_print_u32_hex((uint32_t)ev.key);
        serial_write("\n");
        window_t* target = gui_window_get_focused();
        if (target && ev.window_id && ev.window_id != target->id) {
            // find window by id if needed, for now route to focused
        }

        if (ev.type == EVENT_KEY_DOWN || ev.type == EVENT_KEY_UP) {
            // For demo: if focused window, perhaps move it on arrow keys
            if (target && ev.type == EVENT_KEY_DOWN) {
                if (ev.key == 1003 /*UP*/) gui_window_move(target, 0, -10);
                else if (ev.key == 1004 /*DOWN*/) gui_window_move(target, 0, 10);
                else if (ev.key == 1001 /*LEFT*/) gui_window_move(target, -10, 0);
                else if (ev.key == 1002 /*RIGHT*/) gui_window_move(target, 10, 0);
            }
        } else if (ev.type == EVENT_MOUSE_MOVE) {
            // Future: hit test for focus change
        }
    }
}

void gui_post_key_event(int key, int down) {
    serial_write("EVENTS: post_key_event key=");
    serial_print_u32_hex((uint32_t)key);
    serial_write("\n");
    gui_event_t ev = {0};
    ev.type = down ? EVENT_KEY_DOWN : EVENT_KEY_UP;
    ev.key = key;
    window_t* f = gui_window_get_focused();
    if (f) ev.window_id = f->id;
    serial_write("EVENTS: post got focused, posting event\n");
    gui_post_event(&ev);
    serial_write("EVENTS: post_event returned\n");
}
