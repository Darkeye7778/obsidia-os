#pragma once
#include <stdint.h>

void console_init(uint64_t width, uint64_t height);
void console_putc(char c);
void console_print(const char* str);
void console_move_cursor_left();
void console_move_cursor_right();
void console_move_cursor_up();
void console_move_cursor_down();
void console_set_edit_region_here();
void console_clear_current_line_from_edit_start();
void console_reset();
