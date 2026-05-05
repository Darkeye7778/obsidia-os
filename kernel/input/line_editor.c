#include "line_editor.h"
#include "../console/console.h"
#include "../drivers/keyboard.h"
#include "../shell/shell.h"

#define LINE_MAX 256

static char line[LINE_MAX];
static int len = 0;
static int cursor = 0;

static void redraw_line(void) {
    console_clear_current_line_from_edit_start();

    for (int i = 0; i < len; i++) {
        console_putc(line[i]);
    }

    int moves_left = len - cursor;
    for (int i = 0; i < moves_left; i++) {
        console_move_cursor_left();
    }
}

void line_editor_init(void) {
    len = 0;
    cursor = 0;
}

void line_editor_handle_key(int key) {
    if (key == KEY_ARROW_LEFT) {
        if (cursor > 0) {
            cursor--;
            console_move_cursor_left();
        }
        return;
    }

    if (key == KEY_ARROW_RIGHT) {
        if (cursor < len) {
            cursor++;
            console_move_cursor_right();
        }
        return;
    }

    if (key == '\b') {
        if (cursor > 0) {
            for (int i = cursor - 1; i < len - 1; i++) {
                line[i] = line[i + 1];
            }

            len--;
            cursor--;
            redraw_line();
        }
        return;
    }

    if (key == '\n') {
        console_move_cursor_right();
        console_putc('\n');

        line[len] = 0;

        shell_execute(line);

        console_print("> ");
        console_set_edit_region_here();

        len = 0;
        cursor = 0;
        return;
    }

    if (key >= 32 && key <= 126) {
        if (len < LINE_MAX - 1) {
            for (int i = len; i > cursor; i--) {
                line[i] = line[i - 1];
            }

            line[cursor] = (char)key;
            len++;
            cursor++;

            redraw_line();
        }
    }
}
