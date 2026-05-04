#pragma once
#include <stdint.h>

void console_init(uint64_t width, uint64_t height);
void console_putc(char c);
void console_print(const char* str);

