#include <obsidia/desktop_layout.h>
#include <obsidia/app.h>

static const obs_desktop_layout_t default_layout={
    OBS_BACKGROUND_SUBTLE_BANDS,1,2,
    {{OBS_PANEL_TOP,46,3,{{OBS_PANEL_MODULE_LAUNCHER,184},{OBS_PANEL_MODULE_RUNNING_APPS,0},{OBS_PANEL_MODULE_SPACER,0}}}},
    {{"window-demo" OBS_NATIVE_EXEC_EXTENSION,28,82},{"settings-demo" OBS_NATIVE_EXEC_EXTENSION,28,190}}
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
