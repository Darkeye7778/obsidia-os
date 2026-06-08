#pragma once
#include <stdint.h>

void fb_init(uint32_t* addr, uint64_t width, uint64_t height, uint64_t pitch);

void fb_put_pixel(uint64_t x, uint64_t y, uint32_t color);

void fb_clear(uint32_t color);

void fb_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color);

void fb_draw_char(uint64_t x, uint64_t y, char c, uint32_t fg, uint32_t bg);

void fb_draw_string(uint64_t x, uint64_t y, const char* str, uint32_t fg, uint32_t bg);

// For syscalls / userland
void fb_get_info(uint64_t* width, uint64_t* height, uint64_t* pitch, uint32_t** addr);

// For paging to identity-map the framebuffer MMIO region (may be at high physical addr)
uint64_t fb_get_phys_base(void);
