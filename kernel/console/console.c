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

void console_reset(void) {
    cursor_x = 0;
    cursor_y = 0;
    edit_min_x = 0;
    edit_min_y = 0;
    edit_max_x = 0;
    edit_max_y = 0;

    for (uint64_t y = 0; y < CONSOLE_MAX_ROWS; y++) {
        for (uint64_t x = 0; x < CONSOLE_MAX_COLS; x++) {
            screen_chars[y][x] = ' ';
        }
    }

    console_draw_cursor();
}

// Restore the console's logical text (from screen_chars) into a pixel rectangle.
// This is used by the Phase 3A demo so that when the movable window is moved,
// the shell/console text that was "underneath" it reappears instead of being
// permanently lost (the window compositor draws directly on the fb).
void console_refresh_rect(int64_t px, int64_t py, uint64_t pw, uint64_t ph) {
    if (max_x == 0 || max_y == 0 || pw == 0 || ph == 0) return;

    // Compute the visible on-screen portion of the rect (handles negative px/py
    // when the window has moved off the left or top edge, which was causing
    // trails because the old uint64 cast turned negatives into huge numbers
    // that clamped to the wrong area and did no useful refresh).
    int64_t vis_x = px;
    int64_t vis_y = py;
    uint64_t vis_w = pw;
    uint64_t vis_h = ph;

    if (vis_x < 0) {
        if ((uint64_t)(-vis_x) >= vis_w) return; // entirely off left
        vis_w -= (uint64_t)(-vis_x);
        vis_x = 0;
    }
    if (vis_y < 0) {
        if ((uint64_t)(-vis_y) >= vis_h) return; // entirely off top
        vis_h -= (uint64_t)(-vis_y);
        vis_y = 0;
    }

    // Now convert visible rect to cells and clamp to console bounds
    uint64_t col_start = (uint64_t)vis_x / FONT_WIDTH;
    uint64_t row_start = (uint64_t)vis_y / FONT_HEIGHT;
    uint64_t col_end   = ((uint64_t)vis_x + vis_w + FONT_WIDTH - 1) / FONT_WIDTH;
    uint64_t row_end   = ((uint64_t)vis_y + vis_h + FONT_HEIGHT - 1) / FONT_HEIGHT;

    if (col_start > max_x) col_start = max_x;
    if (row_start > max_y) row_start = max_y;
    if (col_end   > max_x) col_end   = max_x;
    if (row_end   > max_y) row_end   = max_y;

    for (uint64_t r = row_start; r < row_end; r++) {
        for (uint64_t c = col_start; c < col_end; c++) {
            // Redraw using the "normal" console text colors.
            // This restores the original character that was at that cell.
            console_draw_cell(c, r, 0x00FFFFFF, 0x00202020);
        }
    }
}
