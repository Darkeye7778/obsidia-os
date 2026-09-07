#include <obsidia/display.h>
/* Direct client attachment/presentation was retired when displayd became the
 * compositor. Kept as an ABI-compatible denial path for old applications. */
int os_display_attach_surface(uint64_t display,uint64_t surface,uint32_t x,uint32_t y){(void)display;(void)surface;(void)x;(void)y;return-1;}
int os_display_present(uint64_t display){(void)display;return-1;}
