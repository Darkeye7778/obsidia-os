#include "display.h"
#include "drivers/framebuffer.h"
#include "memory/heap.h"
#include "console/console.h"
#include <stdint.h>

static display_info_t disp = {0};

void display_init(void) {
    uint64_t w,h,p; uint32_t* addr;
    fb_get_info(&w, &h, &p, &addr);
    disp.width = w;
    disp.height = h;
    disp.pitch = p;
    disp.front = addr;
    disp.back = 0;
    disp.has_back = 0;
    console_print("Display abstraction initialized\n");
}

void display_get_info(display_info_t* out) {
    if (out) *out = disp;
}

void display_clear(uint32_t color) {
    fb_clear(color);
    if (disp.has_back && disp.back) {
        for (uint64_t i=0; i < disp.width * disp.height; i++) disp.back[i] = color;
    }
}

void display_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
    fb_fill_rect(x, y, w, h, color);
}

void display_blit(uint64_t x, uint64_t y, uint64_t w, uint64_t h, const uint32_t* src, uint64_t src_pitch) {
    // simple copy to front
    for (uint64_t iy=0; iy<h; iy++) {
        for (uint64_t ix=0; ix<w; ix++) {
            fb_put_pixel(x+ix, y+iy, src[iy * src_pitch + ix]);
        }
    }
}

void display_swap_buffers(void) {
    if (!disp.has_back || !disp.back) return;
    // copy back to front (slow but demonstrates)
    for (uint64_t i=0; i < disp.width * disp.height; i++) {
        disp.front[i] = disp.back[i];
    }
}

void display_vsync(void) {
    // placeholder - in real would wait for hardware vsync
}
