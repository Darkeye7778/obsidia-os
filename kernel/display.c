#include "display.h"
#include "drivers/framebuffer.h"
#include "memory/heap.h"
#include "console/console.h"
#include <stdint.h>
#include <stddef.h>

static display_info_t disp = {0};

// Direct framebuffer access for fast bulk drawing (avoids per-pixel function call overhead
// in hot paths like window content blits). This is critical to keep input responsive
// when redrawing the demo window on arrow keys.
static uint32_t* g_direct_fb = NULL;
static uint64_t g_direct_width = 0;
static uint64_t g_direct_height = 0;
static uint64_t g_direct_pitch_pixels = 0;  // pixels per row

void display_init(void) {
    uint64_t w, h, byte_pitch; uint32_t* addr;
    fb_get_info(&w, &h, &byte_pitch, &addr);
    disp.width = w;
    disp.height = h;
    disp.pitch = byte_pitch;
    disp.front = addr;
    disp.back = 0;
    disp.has_back = 0;

    g_direct_fb = addr;
    g_direct_width = w;
    g_direct_height = h;
    g_direct_pitch_pixels = byte_pitch / 4;  // convert bytes to pixels

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

void display_fill_rect(uint64_t ux, uint64_t uy, uint64_t uw, uint64_t uh, uint32_t color) {
    if (!g_direct_fb) {
        fb_fill_rect(ux, uy, uw, uh, color);
        return;
    }

    // Proper clipping for partially off-screen rects (handles negative window coords
    // when the demo window is moved off the left or top edge).
    int64_t x = (int64_t)ux;
    int64_t y = (int64_t)uy;
    int64_t w = (int64_t)uw;
    int64_t h = (int64_t)uh;

    if (w <= 0 || h <= 0) return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int64_t)g_direct_width)  w = (int64_t)g_direct_width  - x;
    if (y + h > (int64_t)g_direct_height) h = (int64_t)g_direct_height - y;

    if (w <= 0 || h <= 0) return;

    uint64_t sx = (uint64_t)x;
    uint64_t sy = (uint64_t)y;
    uint64_t sw = (uint64_t)w;
    uint64_t sh = (uint64_t)h;

    for (uint64_t cy = sy; cy < sy + sh; cy++) {
        uint64_t row = cy * g_direct_pitch_pixels + sx;
        for (uint64_t cx = sx; cx < sx + sw; cx++) {
            g_direct_fb[row + (cx - sx)] = color;
        }
    }
}

void display_blit(uint64_t ux, uint64_t uy, uint64_t uw, uint64_t uh, const uint32_t* src, uint64_t src_pitch) {
    if (!g_direct_fb || !src) {
        // slow fallback
        for (uint64_t iy=0; iy<uh; iy++) {
            for (uint64_t ix=0; ix<uw; ix++) {
                fb_put_pixel(ux+ix, uy+iy, src[iy * src_pitch + ix]);
            }
        }
        return;
    }

    // Clip destination and adjust source offset so that only the visible portion
    // of the window content is blitted when the demo window is partially off-screen.
    // This prevents the client area from appearing "transparent".
    int64_t dx = (int64_t)ux;
    int64_t dy = (int64_t)uy;
    int64_t dw = (int64_t)uw;
    int64_t dh = (int64_t)uh;

    if (dw <= 0 || dh <= 0) return;

    int64_t sx = 0;
    int64_t sy = 0;

    if (dx < 0) {
        sx = -dx;
        dw += dx;
        dx = 0;
    }
    if (dy < 0) {
        sy = -dy;
        dh += dy;
        dy = 0;
    }
    if (dx + dw > (int64_t)g_direct_width)  dw = (int64_t)g_direct_width  - dx;
    if (dy + dh > (int64_t)g_direct_height) dh = (int64_t)g_direct_height - dy;

    if (dw <= 0 || dh <= 0) return;

    uint64_t ddx = (uint64_t)dx;
    uint64_t ddy = (uint64_t)dy;
    uint64_t ddw = (uint64_t)dw;
    uint64_t ddh = (uint64_t)dh;

    for (uint64_t iy = 0; iy < ddh; iy++) {
        uint64_t dst_row = (ddy + iy) * g_direct_pitch_pixels + ddx;
        const uint32_t* src_row = src + (sy + iy) * src_pitch + sx;
        for (uint64_t ix = 0; ix < ddw; ix++) {
            g_direct_fb[dst_row + ix] = src_row[ix];
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
