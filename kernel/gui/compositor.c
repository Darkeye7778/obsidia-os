#include "compositor.h"
#include "window.h"
#include "../display.h"
#include "../console/console.h"
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

    display_clear(0xFF202020); // dark background

    // Traverse from back (end of list) to front would be ideal, but simple list: draw in reverse order by collecting or just draw and let z be managed by caller.
    // For now: draw in list order (head on top will overdraw).

    window_t* w = gui_window_get_list();
    // To draw bottom to top, we can collect or reverse on fly. Simple approach: two pass or accept overdraw order.
    // Better: iterate to find tail, draw from tail to head.
    // For Phase 3A keep simple: just draw in current list order and rely on raise putting important on top.

    while (w) {
        if (w->visible && w->surface) {
            // Blit window surface to display front (using display_blit or direct)
            // Use display primitives for now.
            // For real, we would clip and use dirty regions.
            // Simple full blit for demo:
            // We use the surface buffer directly via fb or display.
            // Since display has no direct surface blit yet, use display_blit if possible or fb_put_pixel loop.
            // To keep it working: use the existing display_fill and a simple loop for window content.

            // For demo purposes, draw a border + title bar using primitives.
            uint32_t border_color = w->focused ? 0xFF00AAFF : 0xFFAAAAAA;
            uint32_t title_bg = w->focused ? 0xFF0055AA : 0xFF555555;

            // Window background via surface clear already done at create, but re-blit content
            // Simple: fill window rect with surface content approximated.
            // Draw title bar
            display_fill_rect(w->x, w->y, w->width, 20, title_bg);
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
