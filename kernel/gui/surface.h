#pragma once
#include <stdint.h>

typedef struct {
    uint32_t id;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;     // bytes per row
    uint32_t* buffer;   // pixel data (ARGB or similar)
    int dirty;          // simple dirty flag for compositor
} surface_t;

void gui_surface_init(void);

// Allocate a new surface (offscreen buffer)
surface_t* gui_surface_create(uint64_t width, uint64_t height);

// Destroy and free
void gui_surface_destroy(surface_t* surf);

// Get raw buffer for direct drawing (or use primitives)
uint32_t* gui_surface_get_buffer(surface_t* surf);

// Mark dirty for compositor
void gui_surface_mark_dirty(surface_t* surf);

// Clear surface
void gui_surface_clear(surface_t* surf, uint32_t color);

// Basic fill rect on surface
void gui_surface_fill_rect(surface_t* surf, uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color);

// Blit from one surface to another (simple)
void gui_surface_blit(surface_t* dst, uint64_t dx, uint64_t dy,
                      surface_t* src, uint64_t sx, uint64_t sy, uint64_t sw, uint64_t sh);

// Text hook (uses existing font for now)
void gui_surface_draw_text(surface_t* surf, uint64_t x, uint64_t y, const char* str, uint32_t fg, uint32_t bg);
