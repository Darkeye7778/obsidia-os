#include "compositor.h"
#include "window.h"
#include "../display.h"
#include "../console/console.h"
#include "../drivers/framebuffer.h"
#include <stdint.h>

void serial_write(const char *str);

static void serial_print_hex64(uint64_t v) {
    char buf[20];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        int d = (v >> ((15 - i) * 4)) & 0xF;
        buf[2 + i] = (d < 10) ? ('0' + d) : ('A' + (d - 10));
    }
    buf[18] = 0;
    serial_write(buf);
}

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

    serial_write("COMPOSITE: called\n");
    window_t* w = gui_window_get_list();
    if (w) serial_write("COMPOSITE: list non-null, will draw\n");
    else serial_write("COMPOSITE: list NULL, no windows!\n");
    int win_count = 0;
    window_t* ww = w;
    while (ww) { win_count++; ww = ww->next; }
    serial_write("COMPOSITE: window count in list = ");
    // print actual decimal count
    if (win_count == 0) {
        serial_write("0\n");
    } else {
        char cbuf[8]; int cp = 0; int n = win_count;
        while (n > 0 && cp < 7) { cbuf[cp++] = '0' + (n % 10); n /= 10; }
        if (cp == 0) cbuf[cp++] = '0';
        for (int k = cp - 1; k >= 0; k--) { char b[2] = {cbuf[k], 0}; serial_write(b); }
        serial_write("\n");
    }
    serial_write("COMPOSITE: about to loop windows\n");
    while (w) {
        serial_write("COMPOSITE: w@");
        serial_print_hex64((uint64_t)w);
        serial_write(" visible=");
        serial_write(w->visible ? "1" : "0");
        serial_write(" surface@");
        serial_print_hex64((uint64_t)w->surface);
        serial_write("\n");
        if (w->visible && w->surface) {
            serial_write("COMPOSITE: drawing one window\n");
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
