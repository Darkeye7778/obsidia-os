#include "compositor.h"
#include "window.h"
#include "../display.h"
#include "../console/console.h"
#include "../drivers/framebuffer.h"
#include <stdint.h>

static display_info_t disp_info;

void gui_compositor_init(void) {
    display_get_info(&disp_info);
    console_print("GUI Compositor initialized\n");
}

void gui_compositor_composite(void) {
    // Simple compositor: clear, then draw visible windows from back to front.
    // Since our list has head as top, we traverse in reverse conceptually by drawing bottom first.
    // For simplicity in Phase 3A: draw all visible windows (later ones on top if list order is maintained by raise).

    // No full clear here so that the shell/console text remains visible outside the demo windows.
    // (Previous full clear was clobbering the shell prompt and editor.)
    // We just draw the window chrome and content on top.

    // Traverse from back (end of list) to front would be ideal, but simple list: draw in reverse order by collecting or just draw and let z be managed by caller.
    // For now: draw in list order (head on top will overdraw).

    window_t* w = gui_window_get_list();
    while (w) {
        if (w->visible && w->surface) {
            // To make the window visible over shell text (no global clear), first clear the window's rect area.
            display_fill_rect(w->x, w->y, w->width, w->height, 0xFF303030);

            // For demo purposes, draw a border + title bar using primitives.
            uint32_t border_color = w->focused ? 0xFF00AAFF : 0xFFAAAAAA;
            uint32_t title_bg = w->focused ? 0xFF0055AA : 0xFF555555;

            // Draw title bar
            display_fill_rect(w->x, w->y, w->width, 20, title_bg);

            // Draw title text (minimal direct fb draw for Phase 3A demo)
            if (w->title[0]) {
                fb_draw_string(w->x + 5, w->y + 6, w->title, 0xFFFFFFFF, title_bg);
            }

            // Draw border
            display_fill_rect(w->x, w->y, w->width, 2, border_color);
            display_fill_rect(w->x, w->y + w->height - 2, w->width, 2, border_color);
            display_fill_rect(w->x, w->y, 2, w->height, border_color);
            display_fill_rect(w->x + w->width - 2, w->y, 2, w->height, border_color);

            // Draw window content from its surface (simple pixel copy via display)
            if (w->surface && w->surface->buffer) {
                // Use display_blit for the client area below title
                display_blit(w->x + 2, w->y + 22,
                             w->width - 4, w->height - 24,
                             w->surface->buffer + (w->surface->width * 0), // approximate
                             w->surface->width);
            }

            // Simple title text hook
            // console style text not direct on display; we skip detailed text for now or use fb_draw if exposed.
        }
        w = w->next;
    }
}

void gui_compositor_present(void) {
    display_swap_buffers();
    display_vsync();
}
