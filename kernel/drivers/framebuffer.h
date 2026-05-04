#pragma once
#include <stdint.h>

void fb_init(uint32_t* addr, uint64_t width, uint64_t height, uint64_t pitch);

void fb_put_pixel(uint64_t x, uint64_t y, uint32_t color);

void fb_clear(uint32_t color);

void fb_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color);
