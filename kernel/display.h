#pragma once
#include <stdint.h>

typedef struct {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;   // bytes per line
    uint32_t* front;  // current visible (Limine fb)
    uint32_t* back;   // optional back buffer
    int has_back;
} display_info_t;

void display_init(void);

void display_get_info(display_info_t* out);

void display_clear(uint32_t color);

void display_fill_rect(uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color);

void display_blit(uint64_t x, uint64_t y, uint64_t w, uint64_t h, const uint32_t* src, uint64_t src_pitch);

void display_swap_buffers(void);  // if double buffered

void display_vsync(void);  // placeholder hook
