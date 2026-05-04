#include "framebuffer.h"
#include "font.h"

static uint32_t* fb;
static uint64_t fb_width;
static uint64_t fb_height;
static uint64_t fb_pitch;

void fb_init(uint32_t* addr, uint64_t width, uint64_t height, uint64_t pitch) {
    fb = addr;
    fb_width = width;
    fb_height = height;
    fb_pitch = pitch / 4;
}

void fb_put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    fb[y * fb_pitch + x] = color;
}

void fb_clear(uint32_t color) {
    for (uint64_t y = 0; y < fb_height; y++) {
        for (uint64_t x = 0; x < fb_width; x++) {
            fb[y * fb_pitch + x] = color;
        }
    }
}

void fb_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
    for (uint64_t iy = 0; iy < h; iy++) {
        for (uint64_t ix = 0; ix < w; ix++) {
            fb_put_pixel(x + ix, y + iy, color);
        }
    }
}

void fb_draw_char(uint64_t x, uint64_t y, char c, uint32_t fg, uint32_t bg) {
    if ((unsigned char)c >= 128) c = '?';

    for (uint64_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = font8x8_basic[(int)c][row];

        for (uint64_t col = 0; col < FONT_WIDTH; col++) {
            uint32_t color = (bits & (1 << (7 - col))) ? fg : bg;
            fb_put_pixel(x + col, y + row, color);
        }
    }
}

void fb_draw_string(uint64_t x, uint64_t y, const char* str, uint32_t fg, uint32_t bg) {
    while (*str) {
        fb_draw_char(x, y, *str, fg, bg);
        x += FONT_WIDTH;
        str++;
    }
}
