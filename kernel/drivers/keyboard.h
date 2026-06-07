#pragma once

#define KEY_ARROW_LEFT   1001
#define KEY_ARROW_RIGHT  1002
#define KEY_ARROW_UP     1003
#define KEY_ARROW_DOWN   1004

#define KEY_CTRL_A       1101
#define KEY_CTRL_C       1103
#define KEY_CTRL_L       1112

void keyboard_init(void);
int keyboard_getkey(void);  // returns next key (blocks with hlt until available after interrupts enabled)
