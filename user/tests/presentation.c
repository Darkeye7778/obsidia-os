#include "syscall.h"
#include <obsidia/app_metadata.h>
#include <obsidia/window.h>
#include <obsidia/internal/desktop.h>
#include <obsidia/desktop_layout.h>
#include <obsidia/gfx.h>
#include <obsidia/theme.h>

static int fail(void){return-1;}
int obsidia_main(void){
    if(sizeof(obs_window_event_t)!=32||sizeof(obs_desktop_management_event_t)!=64)return fail();
    uint32_t guarded[16*12+2];for(uint32_t i=0;i<sizeof(guarded)/sizeof(guarded[0]);i++)guarded[i]=0x13579bdf;uint32_t*pixels=&guarded[1];for(uint32_t i=0;i<16*12;i++)pixels[i]=0;
    obs_canvas_t canvas;obs_canvas_init(&canvas,pixels,16,12,16);obs_canvas_set_clip(&canvas,3,2,6,5);obs_fill_rect(&canvas,-20,-20,100,100,0x00abcdef);
    if(guarded[0]!=0x13579bdf||guarded[16*12+1]!=0x13579bdf)return fail();
    for(uint32_t y=0;y<12;y++)for(uint32_t x=0;x<16;x++)if(pixels[y*16+x]!=(x>=3&&x<9&&y>=2&&y<7?0x00abcdef:0))return fail();
    if(obs_text_width("Ab",2)!=22||obs_text_width("",2)!=0||obs_glyph_height(2)!=14)return fail();
    for(uint32_t i=0;i<16*12;i++)pixels[i]=0;
    obs_canvas_reset_clip(&canvas);obs_draw_text_clipped(&canvas,2,1,4,"WIDE",0x00ffffff,1);
    for(uint32_t y=0;y<12;y++)for(uint32_t x=6;x<16;x++)if(pixels[y*16+x])return fail();
    uint32_t unknown[16*12],question[16*12];for(uint32_t i=0;i<16*12;i++)unknown[i]=question[i]=0;obs_canvas_init(&canvas,unknown,16,12,16);obs_draw_glyph(&canvas,1,1,1,0x00ffffff,1);obs_canvas_init(&canvas,question,16,12,16);obs_draw_glyph(&canvas,1,1,'?',0x00ffffff,1);for(uint32_t i=0;i<16*12;i++)if(unknown[i]!=question[i])return fail();
    const obs_theme_t*normal=obs_theme_get(OBS_THEME_DEFAULT),*alternate=obs_theme_get(OBS_THEME_ALTERNATE);if(!normal||!alternate||normal->colors.panel_background==alternate->colors.panel_background||normal->colors.accent_primary==alternate->colors.accent_primary)return fail();
    obs_desktop_layout_t top=*obs_desktop_layout_default();obs_work_area_t area;int32_t popup_x=0,popup_y=0;uint32_t launcher_height=0,rows=0;top.panels[0].size=60;if(obs_desktop_work_area(&top,1280,720,&area)<0||area.x||area.y!=60||area.width!=1280||area.height!=660||obs_panel_popup_position(&top.panels[0],1280,720,340,176,8,12,&popup_x,&popup_y)<0||popup_x!=12||popup_y!=68)return fail();top.panels[0].edge=OBS_PANEL_BOTTOM;if(obs_desktop_work_area(&top,1280,720,&area)<0||area.y||area.height!=660||obs_panel_popup_position(&top.panels[0],1280,720,340,176,8,12,&popup_x,&popup_y)<0||popup_x!=12||popup_y!=476)return fail();if(obs_launcher_overlay_layout(0,720,&launcher_height,&rows)<0||rows!=1||launcher_height!=112||obs_launcher_overlay_layout(1,720,&launcher_height,&rows)<0||rows!=1||obs_launcher_overlay_layout(32,720,&launcher_height,&rows)<0||rows!=8||launcher_height!=524)return fail();
    static const char manifest[]="manifest-version=1\nid=org.obsidia.test\nname=Test App\nexecutable=test.obsx\nversion=1.0\nicon=application\ncapabilities=none\n";obs_app_metadata_t app;if(obs_app_manifest_parse(manifest,sizeof(manifest)-1,&app)<0||app.icon!=OBS_ICON_APPLICATION||app.capabilities)return fail();
    static const char passed[]="presentation: clipping, text metrics/glyph fallback, themes, metadata, panel work areas passed\n";sys_fd_write(1,passed,sizeof(passed)-1);return 0;
}
