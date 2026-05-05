#include "console.h"
#include "../drivers/framebuffer.h"
#include "../drivers/font.h"

static uint64_t cursor_x = 0;
static uint64_t cursor_y = 0;

static uint64_t max_x;
static uint64_t max_y;

static uint64_t edit_min_x = 0;
static uint64_t edit_min_y = 0;
static uint64_t edit_max_x = 0;
static uint64_t edit_max_y = 0;

#define CONSOLE_MAX_COLS 256
#define CONSOLE_MAX_ROWS 128

static char screen_chars[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];

static char console_get_char(uint64_t x, uint64_t y) {
    if (x >= CONSOLE_MAX_COLS || y >= CONSOLE_MAX_ROWS) {
        return ' ';
    }

    char c = screen_chars[y][x];
    return c ? c : ' ';
}

static void console_set_char(uint64_t x, uint64_t y, char c) {
    if (x >= CONSOLE_MAX_COLS || y >= CONSOLE_MAX_ROWS) {
        return;
    }

    screen_chars[y][x] = c;
}

static void console_draw_cell(uint64_t x, uint64_t y, uint32_t fg, uint32_t bg) {
    char c = console_get_char(x, y);

    fb_draw_char(x * FONT_WIDTH,
                 y * FONT_HEIGHT,
                 c,
                 fg,
                 bg);
}

static void console_draw_cursor(void) {
    console_draw_cell(cursor_x, cursor_y, 0x00000000, 0x00FFFFFF);
}

static void console_clear_cursor(void) {
    console_draw_cell(cursor_x, cursor_y, 0x00FFFFFF, 0x00202020);
}

void console_init(uint64_t width, uint64_t height) {
    max_x = width / FONT_WIDTH;
    max_y = height / FONT_HEIGHT;

    if (max_x > CONSOLE_MAX_COLS) max_x = CONSOLE_MAX_COLS;
    if (max_y > CONSOLE_MAX_ROWS) max_y = CONSOLE_MAX_ROWS;

    cursor_x = 0;
    cursor_y = 0;

    for (uint64_t y = 0; y < CONSOLE_MAX_ROWS; y++) {
        for (uint64_t x = 0; x < CONSOLE_MAX_COLS; x++) {
            screen_chars[y][x] = ' ';
        }
    }

    console_draw_cursor();
}

void console_putc(char c) {
    console_clear_cursor();

    // ===== BACKSPACE =====
    if (c == '\b') {
        // Don't allow deleting before editable region
        if (cursor_y == edit_min_y && cursor_x <= edit_min_x) {
            console_draw_cursor();
            return;
        }

        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > edit_min_y) {
            cursor_y--;
            cursor_x = max_x - 1;
        }

        console_set_char(cursor_x, cursor_y, ' ');
        console_draw_cell(cursor_x, cursor_y, 0x00FFFFFF, 0x00202020);

        if (edit_max_y > cursor_y ||
            (edit_max_y == cursor_y && edit_max_x > cursor_x)) {
            edit_max_x = cursor_x;
            edit_max_y = cursor_y;
        }

        console_draw_cursor();
        return;
    }

    // ===== NEWLINE =====
    if (c == '\n') {
        cursor_x = 0;
        if (cursor_y < max_y - 1) {
            cursor_y++;
        }

        // Expand editable region downward
        if (cursor_y > edit_max_y) {
            edit_max_y = cursor_y;
            edit_max_x = cursor_x;
        }

        console_draw_cursor();
        return;
    }

    // ===== NORMAL CHARACTER =====
    console_set_char(cursor_x, cursor_y, c);
    console_draw_cell(cursor_x, cursor_y, 0x00FFFFFF, 0x00202020);

    // Advance cursor
    if (cursor_x < max_x - 1) {
        cursor_x++;
    } else {
        cursor_x = 0;
        if (cursor_y < max_y - 1) {
            cursor_y++;
        }
    }

    // Update editable max boundary
    if (cursor_y > edit_max_y ||
        (cursor_y == edit_max_y && cursor_x > edit_max_x)) {
        edit_max_x = cursor_x;
        edit_max_y = cursor_y;
    }

    console_draw_cursor();
}

void console_print(const char* str) {
    while (*str) {
        console_putc(*str++);
    }
}

void console_move_cursor_left(void) {
    if (cursor_y == edit_min_y && cursor_x <= edit_min_x) return;

    console_clear_cursor();

    if (cursor_x > 0) {
        cursor_x--;
    } else if (cursor_y > edit_min_y) {
        cursor_y--;
        cursor_x = max_x - 1;
    }

    console_draw_cursor();
}

void console_move_cursor_right(void) {
    if (cursor_y > edit_max_y || (cursor_y == edit_max_y && cursor_x >= edit_max_x)) {
        return;
    }

    console_clear_cursor();

    if (cursor_x < max_x - 1) {
        cursor_x++;
    } else if (cursor_y < max_y - 1) {
        cursor_x = 0;
        cursor_y++;
    }

    console_draw_cursor();
}

void console_move_cursor_up(void) {
    return;
    if (cursor_y > 0) {
        console_clear_cursor();
        cursor_y--;
        console_draw_cursor();
    }
}

void console_move_cursor_down(void) {
    return;
    if (cursor_y < max_y - 1) {
        console_clear_cursor();
        cursor_y++;
        console_draw_cursor();
    }
}

void console_set_edit_region_here(void) {
    edit_min_x = cursor_x;
    edit_min_y = cursor_y;
    edit_max_x = cursor_x;
    edit_max_y = cursor_y;
}

void console_clear_current_line_from_edit_start(void) {
    console_clear_cursor();

    cursor_x = edit_min_x;
    cursor_y = edit_min_y;

    for (uint64_t x = edit_min_x; x < max_x; x++) {
        console_set_char(x, edit_min_y, ' ');
        console_draw_cell(x, edit_min_y, 0x00FFFFFF, 0x00202020);
    }

    edit_max_x = edit_min_x;
    edit_max_y = edit_min_y;

    console_draw_cursor();
}
