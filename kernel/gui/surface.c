#include "surface.h"
#include "../drivers/framebuffer.h"  // for font primitives if needed
#include "../console/console.h"
#include "../memory/heap.h"
#include "../memory/memory.h"
#include "../paging.h"
#include <stdint.h>

void serial_write(const char *str);

static uint32_t next_surface_id = 1;

void gui_surface_init(void) {
    console_print("GUI Surface abstraction initialized\n");
}

surface_t* gui_surface_create(uint64_t width, uint64_t height) {
    serial_write("SURFACE: create enter\n");
    if (width == 0 || height == 0) return 0;

    // pmm for descriptor (single page, always "contiguous")
    surface_t* s = (surface_t*)pmm_alloc_page();
    if (!s) {
        serial_write("SURFACE: pmm struct failed\n");
        return 0;
    }
    for (uint64_t i = 0; i < sizeof(surface_t); i++) ((uint8_t*)s)[i] = 0;

    uint64_t buf_size = width * height * sizeof(uint32_t);
    uint64_t pages = (buf_size + 4095ULL) / 4096ULL;
    uint32_t* buf = (uint32_t*)pmm_alloc_pages(pages);
    if (!buf) {
        serial_write("SURFACE: pmm buffer failed\n");
        pmm_free_page(s);
        return 0;
    }
    s->buffer = buf;
    serial_write("SURFACE: pmm success\n");

    // Ensure identity mapping for this late pmm page (defensive vs pre-map coverage)
    paging_map_page((uint64_t)s, (uint64_t)s, 3);
    for (uint64_t off = 0; off < pages; off++) {
        uint64_t pa = (uint64_t)buf + off * 4096ULL;
        paging_map_page(pa, pa, 3);
    }

    s->id = next_surface_id++;
    s->width = width;
    s->height = height;
    s->pitch = width;  // in pixels for our blit indexing
    s->dirty = 1;

    // Clear to black
    for (uint64_t i = 0; i < width * height; i++) {
        s->buffer[i] = 0xFF000000; // opaque black
    }

    serial_write("SURFACE: about to return s\n");
    return s;
}

void gui_surface_destroy(surface_t* surf) {
    if (!surf) return;
    // For Phase 3A we leak the pmm page(s) for descriptor + buffer (simple, no size tracking needed for buffers).
    // pmm_free_page(surf);  // would be correct if we want to reclaim the descriptor page
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
