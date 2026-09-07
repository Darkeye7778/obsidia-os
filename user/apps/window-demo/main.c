#include "syscall.h"
#include <obsidia/gfx.h>
#include <obsidia/theme.h>
#include <obsidia/settings.h>
#include <obsidia/window.h>

static const obs_theme_t*theme;
static obs_theme_t active_theme;
static char*append_u32(char*out,uint32_t value){char reversed[10];uint32_t n=0;if(!value)*out++='0';else{while(value&&n<10){reversed[n++]=(char)('0'+value%10);value/=10;}while(n)*out++=reversed[--n];}return out;}
static void dimensions(char*out,uint32_t width,uint32_t height){out=append_u32(out,width);*out++=' ';*out++='x';*out++=' ';out=append_u32(out,height);*out=0;}
static void draw(obs_window_t*w,uint32_t count,uint32_t last_key,int focused){
    obs_canvas_t canvas;obs_canvas_init(&canvas,w->pixels,w->width,w->height,w->width);obs_fill_rect(&canvas,0,0,(int32_t)w->width,(int32_t)w->height,theme->colors.window_background);
    uint32_t margin=theme->metrics.spacing_large,scale=theme->metrics.text_scale;obs_draw_text_clipped(&canvas,(int32_t)margin,(int32_t)margin,w->width-margin*2,"Obsidia Window Demo",theme->colors.text_primary,scale);
    char size_text[24];dimensions(size_text,w->width,w->height);obs_draw_text_clipped(&canvas,(int32_t)margin,(int32_t)(margin+24),w->width-margin*2,size_text,theme->colors.text_secondary,scale);
    obs_draw_text_clipped(&canvas,(int32_t)margin,(int32_t)(margin+48),w->width-margin*2,focused?"Focused - type any key":"Click the window to focus",focused?theme->colors.accent_secondary:theme->colors.text_disabled,scale);
    int32_t box_y=(int32_t)(margin+76),box_h=(int32_t)w->height-box_y-(int32_t)margin;if(box_h>20){obs_stroke_rect(&canvas,(int32_t)margin,box_y,(int32_t)w->width-(int32_t)margin*2,box_h,2,focused?theme->colors.accent_primary:theme->colors.window_border_inactive);for(int32_t x=(int32_t)margin+24;x<(int32_t)w->width-(int32_t)margin;x+=48)obs_vline(&canvas,x,box_y+2,box_h-4,theme->colors.panel_border);for(int32_t y=box_y+24;y<box_y+box_h;y+=32)obs_hline(&canvas,(int32_t)margin+2,y,(int32_t)w->width-(int32_t)margin*2-4,theme->colors.panel_border);}
    char reaction[32]="Key reactions: ";char*end=reaction+15;end=append_u32(end,count);*end=0;obs_draw_text_clipped(&canvas,(int32_t)(margin+8),box_y+10,w->width-margin*2-16,reaction,theme->colors.text_primary,scale);
    if(last_key>=32&&last_key<=126){char key[16]="Last key: ";key[10]=(char)last_key;key[11]=0;obs_draw_text_clipped(&canvas,(int32_t)(margin+8),box_y+34,w->width-margin*2-16,key,theme->colors.accent_secondary,scale);}
    os_window_present(w);
}
int obsidia_main(void){
    uint32_t theme_id=OBS_THEME_DEFAULT,text_scale=2;obs_settings_t settings;obs_setting_value_t value;if(obs_settings_connect(&settings)==0){if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_THEME,&value)==0)theme_id=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_TEXT_SCALE,&value)==0)text_scale=value.u32;obs_settings_subscribe(&settings,OBS_SETTINGS_SUBSCRIBE_APPEARANCE);}active_theme=*obs_theme_get((obs_theme_id_t)theme_id);active_theme.metrics.text_scale=text_scale;theme=&active_theme;
    obs_window_t window;if(os_window_create(&window,480,300,"Obsidia Window Demo")<0)return 1;uint32_t count=0,last=0;int focused=1;draw(&window,count,last,focused);static const char ready[]="window-demo: live system theme ready\n";sys_fd_write(1,ready,sizeof(ready)-1);
    for(;;){int processed=0,redraw=0;obs_settings_event_t changed;for(uint32_t i=0;i<8&&obs_settings_try_next(&settings,&changed)>0;i++){processed=redraw=1;if(changed.setting_id==OBS_SETTING_APPEARANCE_THEME){theme_id=changed.value.enumeration;active_theme=*obs_theme_get((obs_theme_id_t)theme_id);active_theme.metrics.text_scale=text_scale;}else if(changed.setting_id==OBS_SETTING_APPEARANCE_TEXT_SCALE){text_scale=changed.value.u32;active_theme.metrics.text_scale=text_scale;}}
        for(uint32_t i=0;i<32;i++){obs_window_event_t event;int n=os_window_try_next_event(&window,&event);if(n<=0)break;processed=redraw=1;if(event.type==OBS_WINDOW_EVENT_CLOSE){os_window_close(&window);obs_settings_close(&settings);static const char closed[]="window-demo: graceful close complete\n";sys_fd_write(1,closed,sizeof(closed)-1);return 0;}if(event.type==OBS_WINDOW_EVENT_FOCUS)focused=event.value;else if(event.type==OBS_WINDOW_EVENT_KEY&&event.value){count++;last=event.code;}}
        if(redraw)draw(&window,count,last,focused);
        if(processed)sys_yield();else sys_sleep(1);
    }
}
