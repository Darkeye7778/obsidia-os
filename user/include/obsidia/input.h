#pragma once
#include <stdint.h>

typedef enum { OBS_INPUT_KEY=1, OBS_INPUT_RELATIVE=2, OBS_INPUT_BUTTON=3, OBS_INPUT_MOUSE_MOVE=4 } obs_input_type_t;
typedef enum { OBS_INPUT_AXIS_X=1, OBS_INPUT_AXIS_Y=2 } obs_input_axis_t;
typedef enum { OBS_INPUT_BUTTON_LEFT=1, OBS_INPUT_BUTTON_RIGHT=2, OBS_INPUT_BUTTON_MIDDLE=3 } obs_input_button_t;

/* KEY: code is an ASCII/special key and value is 1 press or 0 release.
 * RELATIVE: code is an axis and value is a signed delta.
 * BUTTON: code is a button and value is 1 press or 0 release.
 * MOUSE_MOVE: value is dx and reserved is the signed dy bit pattern. */
typedef struct {
    uint32_t type;
    uint32_t code;
    int32_t value;
    uint32_t reserved;
    uint64_t ticks;
} obs_input_event_t;
