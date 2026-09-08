#include <obsidia/desktop_layout.h>
#include <obsidia/app.h>

static const obs_desktop_layout_t default_layout={
    .background_mode=OBS_BACKGROUND_SUBTLE_BANDS,.panel_count=1,.shortcut_count=2,
    .panels={{OBS_PANEL_TOP,46,3,{{OBS_PANEL_MODULE_LAUNCHER,184},{OBS_PANEL_MODULE_RUNNING_APPS,0},{OBS_PANEL_MODULE_SPACER,0}}}},
    .shortcuts={{"window-demo" OBS_NATIVE_EXEC_EXTENSION,28,82},{"settings-demo" OBS_NATIVE_EXEC_EXTENSION,28,190}}
};
const obs_desktop_layout_t *obs_desktop_layout_default(void){return &default_layout;}
int obs_panel_geometry(const obs_panel_config_t *panel,uint32_t width,uint32_t height,obs_work_area_t *g){
    if(!panel||!g||!panel->size||panel->edge<OBS_PANEL_TOP||panel->edge>OBS_PANEL_RIGHT)return-1;
    if((panel->edge==OBS_PANEL_TOP||panel->edge==OBS_PANEL_BOTTOM)&&panel->size>height)return-1;
    if((panel->edge==OBS_PANEL_LEFT||panel->edge==OBS_PANEL_RIGHT)&&panel->size>width)return-1;
    *g=(obs_work_area_t){0,0,width,height};
    if(panel->edge==OBS_PANEL_TOP)g->height=panel->size;
    else if(panel->edge==OBS_PANEL_BOTTOM){g->y=(int32_t)(height-panel->size);g->height=panel->size;}
    else if(panel->edge==OBS_PANEL_LEFT)g->width=panel->size;
    else{g->x=(int32_t)(width-panel->size);g->width=panel->size;}return 0;
}
int obs_desktop_work_area(const obs_desktop_layout_t *layout,uint32_t width,uint32_t height,obs_work_area_t *area){
    if(!layout||!area||!width||!height||layout->panel_count>OBS_LAYOUT_MAX_PANELS)return-1;
    *area=(obs_work_area_t){0,0,width,height};
    for(uint32_t i=0;i<layout->panel_count;i++){const obs_panel_config_t*p=&layout->panels[i];obs_work_area_t g;if(obs_panel_geometry(p,width,height,&g)<0)return-1;
        if(p->edge==OBS_PANEL_TOP){area->y+=(int32_t)p->size;area->height-=p->size;}else if(p->edge==OBS_PANEL_BOTTOM)area->height-=p->size;
        else if(p->edge==OBS_PANEL_LEFT){area->x+=(int32_t)p->size;area->width-=p->size;}else area->width-=p->size;
    }return area->width&&area->height?0:-1;
}
int obs_panel_popup_position(const obs_panel_config_t *panel,uint32_t width,uint32_t height,uint32_t popup_width,uint32_t popup_height,uint32_t gap,uint32_t edge_offset,int32_t *x,int32_t *y){
    obs_work_area_t geometry;
    if(!x||!y||!popup_width||!popup_height||popup_width>width||popup_height>height||
       obs_panel_geometry(panel,width,height,&geometry)<0||
       (panel->edge!=OBS_PANEL_TOP&&panel->edge!=OBS_PANEL_BOTTOM))return-1;
    uint64_t horizontal=(uint64_t)geometry.x+edge_offset;
    if(horizontal+popup_width>width)return-1;
    int64_t vertical=panel->edge==OBS_PANEL_TOP?(int64_t)geometry.y+geometry.height+gap:(int64_t)geometry.y-popup_height-gap;
    if(vertical<0||(uint64_t)vertical+popup_height>height)return-1;
    *x=(int32_t)horizontal;*y=(int32_t)vertical;return 0;
}
int obs_launcher_overlay_layout(uint32_t count,uint32_t screen_height,uint32_t*height,uint32_t*rows){
    if(!height||!rows||screen_height<200)return-1;
    uint32_t visible=count?count:1;
    if(visible>8)visible=8;
    uint32_t value=44+visible*56+(count>visible?32:12);
    if(value>screen_height-16)return-1;
    *height=value;*rows=visible;return 0;
}
