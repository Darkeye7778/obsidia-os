#include "surface.h"
#include "../drivers/framebuffer.h"  // for font primitives if needed
#include "../console/console.h"
#include "../memory/heap.h"
#include <stdint.h>

static uint32_t next_surface_id = 1;

void gui_surface_init(void) {
    console_print("GUI Surface abstraction initialized\n");
}

surface_t* gui_surface_create(uint64_t width, uint64_t height) {
    if (width == 0 || height == 0) return 0;

    surface_t* s = (surface_t*)kmalloc(sizeof(surface_t));
    if (!s) return 0;

    uint64_t buf_size = width * height * sizeof(uint32_t);
    s->buffer = (uint32_t*)kmalloc(buf_size);
    if (!s->buffer) {
        // kfree(s);
        return 0;
    }

    s->id = next_surface_id++;
    s->width = width;
    s->height = height;
    s->pitch = width * sizeof(uint32_t);
    s->dirty = 1;

    // Clear to black
    for (uint64_t i = 0; i < width * height; i++) {
        s->buffer[i] = 0xFF000000; // opaque black
    }

    return s;
}

void gui_surface_destroy(surface_t* surf) {
    if (!surf) return;
    if (surf->buffer) {
        // kfree(surf->buffer);
    }
    // kfree(surf);
}

uint32_t* gui_surface_get_buffer(surface_t* surf) {
    return surf ? surf->buffer : 0;
}

void gui_surface_mark_dirty(surface_t* surf) {
    if (surf) surf->dirty = 1;
}

void gui_surface_clear(surface_t* surf, uint32_t color) {
    if (!surf || !surf->buffer) return;
    for (uint64_t i = 0; i < surf->width * surf->height; i++) {
        surf->buffer[i] = color;
    }
    surf->dirty = 1;
}

void gui_surface_fill_rect(surface_t* surf, uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
    if (!surf || !surf->buffer) return;
    if (x >= surf->width || y >= surf->height) return;

    uint64_t max_x = (x + w > surf->width) ? surf->width : x + w;
    uint64_t max_y = (y + h > surf->height) ? surf->height : y + h;

    for (uint64_t cy = y; cy < max_y; cy++) {
        for (uint64_t cx = x; cx < max_x; cx++) {
            surf->buffer[cy * surf->width + cx] = color;
        }
    }
    surf->dirty = 1;
}

void gui_surface_blit(surface_t* dst, uint64_t dx, uint64_t dy,
                      surface_t* src, uint64_t sx, uint64_t sy, uint64_t sw, uint64_t sh) {
    if (!dst || !src || !dst->buffer || !src->buffer) return;

    for (uint64_t iy = 0; iy < sh; iy++) {
        uint64_t syy = sy + iy;
        uint64_t dyy = dy + iy;
        if (syy >= src->height || dyy >= dst->height) continue;
        for (uint64_t ix = 0; ix < sw; ix++) {
            uint64_t sxx = sx + ix;
            uint64_t dxx = dx + ix;
            if (sxx >= src->width || dxx >= dst->width) continue;
            dst->buffer[dyy * dst->width + dxx] = src->buffer[syy * src->width + sxx];
        }
    }
    dst->dirty = 1;
}

void gui_surface_draw_text(surface_t* surf, uint64_t x, uint64_t y, const char* str, uint32_t fg, uint32_t bg) {
    if (!surf || !str) return;
    // Reuse existing fb text drawing logic, but target the surface buffer.
    // For Phase 3A simplicity, we implement a basic version or hook.
    // Here we do a simple character blitter using font if available, but to keep build simple
    // we fall back to a placeholder that fills a rect for the text area.
    // In practice, later phases or user can improve.
    // For now: draw a background rect and approximate text by filling chars area.
    uint64_t len = 0; while (str[len]) len++;
    gui_surface_fill_rect(surf, x, y, len * 8, 8, bg);
    // Real text would use font8x8 here; we leave a hook comment.
    // For demo we just mark dirty.
    surf->dirty = 1;
}
