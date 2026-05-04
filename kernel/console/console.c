#include "console.h"
#include "../drivers/framebuffer.h"
#include "../drivers/font.h"

static uint64_t cursor_x = 0;
static uint64_t cursor_y = 0;

static uint64_t max_x;
static uint64_t max_y;

void console_init(uint64_t width, uint64_t height) {
    max_x = width / FONT_WIDTH;
    max_y = height / FONT_HEIGHT;
}

void console_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        return;
    }

    fb_draw_char(cursor_x * FONT_WIDTH,
                 cursor_y * FONT_HEIGHT,
                 c,
                 0x00FFFFFF,
                 0x00202020);

    cursor_x++;

    if (cursor_x >= max_x) {
        cursor_x = 0;
        cursor_y++;
    }
}

void console_print(const char* str) {
    while (*str) {
        console_putc(*str++);
    }
}
