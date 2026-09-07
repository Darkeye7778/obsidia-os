#pragma once

#include <stdint.h>

/* Legacy pre-compositor API. Direct client-surface attachment is disabled;
 * new applications must use <obsidia/window.h>. */
int os_display_attach_surface(
    uint64_t display,
    uint64_t surface,
    uint32_t x,
    uint32_t y
);

/*
 * Ask displayd to present the currently attached surface.
 */
int os_display_present(
    uint64_t display
);
