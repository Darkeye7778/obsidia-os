#pragma once
#include <stdint.h>

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_BUTTON,
    EVENT_WINDOW_FOCUS,
    EVENT_WINDOW_CLOSE
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    uint32_t window_id;   // target window if applicable
    int32_t x, y;         // for mouse
    int key;              // for keyboard (reuse existing key codes)
    int button;           // mouse button
    uint32_t mods;        // modifiers
} gui_event_t;

void gui_events_init(void);

// Post an event (from IRQ or other)
void gui_post_event(gui_event_t* ev);

// Get next event for focused or specific window (non-blocking for now)
int gui_get_event(gui_event_t* out);

// Process events for focused window (simple dispatch)
void gui_process_events(void);

// Helper to post keyboard from existing keyboard driver
void gui_post_key_event(int key, int down);
