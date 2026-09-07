#pragma once
#include <stdint.h>

typedef struct {
    uint32_t *pixels;
    uint32_t width,height,stride;
    int32_t clip_x,clip_y;
    uint32_t clip_width,clip_height;
} obs_canvas_t;

void obs_canvas_init(obs_canvas_t *canvas,uint32_t *pixels,uint32_t width,uint32_t height,uint32_t stride);
void obs_canvas_set_clip(obs_canvas_t *canvas,int32_t x,int32_t y,uint32_t width,uint32_t height);
void obs_canvas_reset_clip(obs_canvas_t *canvas);
void obs_fill_rect(obs_canvas_t *canvas,int32_t x,int32_t y,int32_t width,int32_t height,uint32_t color);
void obs_stroke_rect(obs_canvas_t *canvas,int32_t x,int32_t y,int32_t width,int32_t height,uint32_t thickness,uint32_t color);
void obs_hline(obs_canvas_t *canvas,int32_t x,int32_t y,int32_t width,uint32_t color);
void obs_vline(obs_canvas_t *canvas,int32_t x,int32_t y,int32_t height,uint32_t color);
void obs_blit(obs_canvas_t *canvas,int32_t x,int32_t y,uint32_t width,uint32_t height,const uint32_t *source,uint32_t source_stride);
uint32_t obs_text_width(const char *text,uint32_t scale);
uint32_t obs_glyph_width(uint32_t scale);
uint32_t obs_glyph_height(uint32_t scale);
void obs_draw_glyph(obs_canvas_t *canvas,int32_t x,int32_t y,unsigned char glyph,uint32_t color,uint32_t scale);
void obs_draw_text(obs_canvas_t *canvas,int32_t x,int32_t y,const char *text,uint32_t color,uint32_t scale);
void obs_draw_text_clipped(obs_canvas_t *canvas,int32_t x,int32_t y,uint32_t max_width,const char *text,uint32_t color,uint32_t scale);

typedef enum { OBS_ICON_SYSTEM=1,OBS_ICON_WINDOW_DEMO=2,OBS_ICON_APPLICATION=3,OBS_ICON_SETTINGS=4 } obs_icon_id_t;
void obs_draw_icon(obs_canvas_t *canvas,int32_t x,int32_t y,uint32_t size,obs_icon_id_t icon,uint32_t foreground,uint32_t accent);
