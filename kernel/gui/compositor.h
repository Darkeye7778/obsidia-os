#pragma once

void gui_compositor_init(void);

// Composite all visible windows to the display (using current display abstraction)
void gui_compositor_composite(void);

// Present (swap if double buffered)
void gui_compositor_present(void);
