#include "syscall.h"
#include <obsidia/service.h>
#include <obsidia/input.h>
#include <obsidia/gfx.h>
#include <obsidia/theme.h>
#include <obsidia/settings.h>
#include <obsidia/window.h>
#include <obsidia/internal/desktop.h>
#include <obsidia/internal/display_protocol.h>

#define MAX_WINDOWS 16
#define DAMAGE_CAPACITY 16
#define RESIZE_HIT 6
#define MIN_CLIENT_WIDTH 120U
#define MIN_CLIENT_HEIGHT 80U
#define OUTPUT_MAP 0x0000004000000000ULL
#define ROOT_MAP 0x0000004100000000ULL
#define CLIENT_MAP_BASE 0x0000004200000000ULL
#define PENDING_MAP_BASE 0x0000004300000000ULL
#define CLIENT_MAP_STRIDE 0x01000000ULL
#define ROOT_WINDOW_ID 0xffffffffU

#define RESIZE_LEFT 1U
#define RESIZE_RIGHT 2U
#define RESIZE_TOP 4U
#define RESIZE_BOTTOM 8U

typedef struct {
    uint8_t used,focused;
    uint32_t id,width,height,z,generation,state,pre_minimize_state;
    int32_t x,y,restore_x,restore_y;
    uint32_t restore_width,restore_height;
    int64_t surface,session;
    uint64_t owner_pid,mapping_address,session_missing_since;
    const uint32_t*pixels;
    char title[32];
    uint8_t pending;
    uint32_t pending_transaction,pending_width,pending_height,pending_state;
    uint64_t pending_since;
    int32_t pending_x,pending_y;
    int64_t pending_surface;
    uint64_t pending_mapping;
    const uint32_t*pending_pixels;
    uint8_t pending_save_restore;
} window_slot_t;

typedef struct {int32_t x,y,w,h;} damage_rect_t;

static window_slot_t windows[MAX_WINDOWS];
static const obs_theme_t*theme;
static obs_theme_t active_theme;
static obs_settings_t settings;
static uint32_t active_theme_id=OBS_THEME_DEFAULT,active_text_scale=2;
static obs_canvas_t output_canvas;
static uint32_t slot_generations[MAX_WINDOWS];
static damage_rect_t damage[DAMAGE_CAPACITY];
static uint32_t damage_count,screen_width,screen_height,next_z=1,live_windows,next_transaction=1;
static uint8_t full_damage;
static uint64_t damage_events,full_damage_fallbacks,rejected_requests,resize_commits,resize_aborts,input_events;
static int64_t output_surface=-1,output_capability=-1,root_surface=-1,root_session=-1;
static uint64_t root_owner_pid,root_mapping_address,root_session_missing_since;
static const uint32_t*root_pixels;
static uint32_t root_id;
static uint32_t*output_pixels;
static int32_t work_x,work_y;
static uint32_t work_width,work_height;
static int focused=-1,dragged=-1,resizing=-1,hovered=-1,pressed=-1;
static uint8_t resize_direction,left_down,hovered_control,pressed_control;
static int32_t cursor_x,cursor_y,drag_dx,drag_dy,resize_start_x,resize_start_y;
static int32_t resize_initial_x,resize_initial_y,resize_candidate_x,resize_candidate_y;
static uint32_t resize_initial_width,resize_initial_height,resize_candidate_width,resize_candidate_height;

static void write(const char*s);
static void mark_full_damage(void);
static void select_theme(void){active_theme=*obs_theme_get((obs_theme_id_t)active_theme_id);active_theme.metrics.text_scale=active_text_scale;theme=&active_theme;}
static void connect_settings(void){obs_setting_value_t value;if(obs_settings_connect(&settings)<0)return;if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_THEME,&value)==0)active_theme_id=value.enumeration;if(obs_settings_get(&settings,OBS_SETTING_APPEARANCE_TEXT_SCALE,&value)==0)active_text_scale=value.u32;select_theme();obs_settings_subscribe(&settings,OBS_SETTINGS_SUBSCRIBE_APPEARANCE);}
static void poll_settings(void){obs_settings_event_t event;int changed=0;while(obs_settings_try_next(&settings,&event)>0){if(event.setting_id==OBS_SETTING_APPEARANCE_THEME){active_theme_id=event.value.enumeration;changed=1;}else if(event.setting_id==OBS_SETTING_APPEARANCE_TEXT_SCALE){active_text_scale=event.value.u32;changed=1;}}if(changed){select_theme();mark_full_damage();write("displayd: live theme updated\n");}}

static void write(const char*s){uint64_t n=0;while(s[n])n++;sys_fd_write(1,s,n);}
static int visible(const window_slot_t*w){return w->used&&w->state!=OBS_WINDOW_MINIMIZED;}
static int32_t border(void){return(int32_t)theme->metrics.border_width;}
static int32_t title_height(void){return(int32_t)theme->metrics.titlebar_height;}
static int32_t control_size(void){return(int32_t)theme->metrics.control_size;}
static int32_t control_gap(void){return(int32_t)theme->metrics.control_gap;}
static int32_t control_margin(void){return(int32_t)theme->metrics.control_margin;}

static void mark_full_damage(void){damage_events++;full_damage=1;damage_count=0;}
static void mark_damage(int32_t x,int32_t y,int32_t w,int32_t h){
    damage_events++;if(full_damage||w<=0||h<=0)return;
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(x>=(int32_t)screen_width||y>=(int32_t)screen_height)return;
    if(x+w>(int32_t)screen_width)w=(int32_t)screen_width-x;
    if(y+h>(int32_t)screen_height)h=(int32_t)screen_height-y;
    if(w<=0||h<=0)return;
    if(damage_count==DAMAGE_CAPACITY){full_damage=1;damage_count=0;full_damage_fallbacks++;return;}
    damage[damage_count++]=(damage_rect_t){x,y,w,h};
}
static void mark_geometry_damage(int32_t x,int32_t y,uint32_t width,uint32_t height){int32_t shadow=(int32_t)theme->metrics.shadow_size;mark_damage(x-shadow,y-shadow,(int32_t)width+border()*2+shadow*2,(int32_t)height+title_height()+border()+shadow*2);}
static void mark_window_damage(const window_slot_t*w){if(visible(w))mark_geometry_damage(w->x,w->y,w->width,w->height);}

static void clear(uint32_t color){obs_fill_rect(&output_canvas,0,0,(int32_t)screen_width,(int32_t)screen_height,color);}
static void rect(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t color){obs_fill_rect(&output_canvas,x,y,w,h,color);}
static void pixel(int32_t x,int32_t y,uint32_t color){obs_fill_rect(&output_canvas,x,y,1,1,color);}
static void blit(int32_t x,int32_t y,uint32_t width,uint32_t height,const uint32_t*source){obs_blit(&output_canvas,x,y,width,height,source,width);}

static int32_t control_x(const window_slot_t*w,uint32_t from_right){return w->x+border()+(int32_t)w->width-control_margin()-control_size()-(int32_t)from_right*(control_size()+control_gap());}
static int32_t control_y(const window_slot_t*w){return w->y+border()+(title_height()-control_size())/2;}
static void draw_controls(const window_slot_t*w){
    int32_t y=control_y(w),s=control_size();
    for(uint32_t control=1;control<=3;control++){int32_t x=control_x(w,3-control);uint32_t color=control==3?theme->colors.control_danger:theme->colors.control_normal;
        if(hovered>=0&& &windows[hovered]==w&&hovered_control==control)color=control==3?theme->colors.control_danger_hover:theme->colors.control_hover;
        if(pressed>=0&& &windows[pressed]==w&&pressed_control==control)color=theme->colors.control_pressed;
        rect(x,y,s,s,color);
        uint32_t glyph=theme->colors.title_text_active;
        if(control==1)rect(x+5,y+s-6,s-10,2,glyph);
        else if(control==2){obs_stroke_rect(&output_canvas,x+5,y+4,s-10,s-9,1,glyph);if(w->state==OBS_WINDOW_MAXIMIZED)obs_stroke_rect(&output_canvas,x+3,y+6,s-10,s-9,1,glyph);}
        else for(int32_t i=5;i<s-5;i++){pixel(x+i,y+i,glyph);pixel(x+s-1-i,y+i,glyph);}
    }
}
static void draw_cursor(void){for(int32_t y=0;y<18;y++)for(int32_t x=0;x<=y/2&&x<9;x++){uint32_t color=(x==0||x==y/2||y==17)?theme->colors.window_shadow:theme->colors.selection_foreground;pixel(cursor_x+x,cursor_y+y,color);}}
static void draw_resize_outline(void){
    if(resizing<0)return;
    int32_t w=(int32_t)resize_candidate_width+border()*2,h=(int32_t)resize_candidate_height+title_height()+border();
    obs_stroke_rect(&output_canvas,resize_candidate_x,resize_candidate_y,w,h,2,theme->colors.accent_secondary);
}
static int next_window_by_z(uint32_t after){int best=-1;uint32_t best_z=0xffffffffU;for(int i=0;i<MAX_WINDOWS;i++)if(visible(&windows[i])&&windows[i].z>after&&windows[i].z<best_z){best=i;best_z=windows[i].z;}return best;}
static void repaint(void){
    if(!full_damage&&!damage_count)return;
    if(root_pixels)blit(0,0,screen_width,screen_height,root_pixels);else clear(theme->colors.desktop_background);
    uint32_t z=0;for(;;){int index=next_window_by_z(z);if(index<0)break;window_slot_t*w=&windows[index];z=w->z;
        int32_t outer_w=(int32_t)w->width+border()*2,outer_h=(int32_t)w->height+title_height()+border(),shadow=(int32_t)theme->metrics.shadow_size;
        rect(w->x+shadow,w->y+shadow,outer_w,outer_h,theme->colors.window_shadow);
        rect(w->x,w->y,outer_w,outer_h,w->focused?theme->colors.window_border_active:theme->colors.window_border_inactive);
        rect(w->x+border(),w->y+border(),(int32_t)w->width,title_height()-border(),w->focused?theme->colors.titlebar_active:theme->colors.titlebar_inactive);
        int32_t title_x=w->x+border()+(int32_t)theme->metrics.spacing_medium;
        int32_t title_y=w->y+border()+(title_height()-border()-(int32_t)obs_glyph_height(theme->metrics.text_scale))/2;
        int32_t controls_width=control_margin()+3*control_size()+2*control_gap();
        int32_t available=w->x+border()+(int32_t)w->width-controls_width-title_x;
        if(available>0)obs_draw_text_clipped(&output_canvas,title_x,title_y,(uint32_t)available,w->title,w->focused?theme->colors.title_text_active:theme->colors.title_text_inactive,theme->metrics.text_scale);
        draw_controls(w);blit(w->x+border(),w->y+title_height(),w->width,w->height,w->pixels);
    }
    draw_resize_outline();draw_cursor();os_surface_present((uint64_t)output_capability,(uint64_t)output_surface,0,0);damage_count=0;full_damage=0;
}

static void send_event(int index,uint32_t type,uint32_t code,int32_t value,int32_t x,int32_t y,uint64_t ticks){
    obs_window_event_t event={0};event.type=type;event.code=code;event.value=value;event.x=x;event.y=y;event.ticks=ticks;
    if(index>=0&&index<MAX_WINDOWS&&windows[index].used)os_ipc_try_send((uint64_t)windows[index].session,&event,sizeof(event));else if(index==-2&&root_session>=0)os_ipc_try_send((uint64_t)root_session,&event,sizeof(event));
}
static void notify_root(uint32_t type,uint32_t id,uint32_t state){
    obs_desktop_management_event_t event={0};event.type=type;event.window_id=id;event.state=state;event.ticks=sys_getticks();
    for(int i=0;i<MAX_WINDOWS;i++)if(windows[i].used&&windows[i].id==id){for(uint32_t n=0;n<sizeof(event.title)-1&&windows[i].title[n];n++)event.title[n]=windows[i].title[n];break;}
    if(root_session>=0)os_ipc_try_send((uint64_t)root_session,&event,sizeof(event));
}
static int top_live_window(void){int best=-1;uint32_t best_z=0;for(int i=0;i<MAX_WINDOWS;i++)if(visible(&windows[i])&&windows[i].z>=best_z&&os_process_alive(windows[i].owner_pid)>0){best=i;best_z=windows[i].z;}return best;}
static void set_focus(int index,uint64_t ticks){
    if(index>=0&&!visible(&windows[index]))index=-1;
    if(index==focused)return;
    if(focused>=0&&windows[focused].used){mark_window_damage(&windows[focused]);windows[focused].focused=0;send_event(focused,OBS_WINDOW_EVENT_FOCUS,0,0,0,0,ticks);}
    focused=index;if(index>=0&&windows[index].used){windows[index].focused=1;windows[index].z=next_z++;mark_window_damage(&windows[index]);send_event(index,OBS_WINDOW_EVENT_FOCUS,0,1,0,0,ticks);write("displayd: window focused and raised\n");}
    notify_root(OBS_DESKTOP_EVENT_WINDOW_FOCUSED,index>=0?windows[index].id:0,index>=0?windows[index].state:0);
}
static int id_index(uint32_t id){uint32_t encoded=id&31U;if(!encoded||encoded>MAX_WINDOWS)return-1;int index=(int)encoded-1;return windows[index].used&&windows[index].id==id?index:-1;}
static int find_window(uint32_t id,uint64_t owner){int index=id_index(id);return index>=0&&windows[index].owner_pid==owner?index:-1;}
static int hit_test(int32_t x,int32_t y){int best=-1;uint32_t best_z=0;for(int i=0;i<MAX_WINDOWS;i++)if(visible(&windows[i])&&windows[i].z>=best_z){int32_t width=(int32_t)windows[i].width+border()*2,height=(int32_t)windows[i].height+title_height()+border();if(x>=windows[i].x&&y>=windows[i].y&&x<windows[i].x+width&&y<windows[i].y+height){best=i;best_z=windows[i].z;}}return best;}
static int hit_box(int32_t bx,int32_t by,int32_t x,int32_t y){return x>=bx&&y>=by&&x<bx+control_size()&&y<by+control_size();}
static uint32_t hit_control(const window_slot_t*w,int32_t x,int32_t y){int32_t cy=control_y(w);if(hit_box(control_x(w,0),cy,x,y))return 3;if(hit_box(control_x(w,1),cy,x,y))return 2;if(hit_box(control_x(w,2),cy,x,y))return 1;return 0;}
static void update_hover(void){int next=hit_test(cursor_x,cursor_y);uint8_t control=next>=0?hit_control(&windows[next],cursor_x,cursor_y):0;if(!control)next=-1;if(next==hovered&&control==hovered_control)return;if(hovered>=0&&windows[hovered].used)mark_window_damage(&windows[hovered]);hovered=next;hovered_control=control;if(hovered>=0)mark_window_damage(&windows[hovered]);}
static uint8_t hit_resize(const window_slot_t*w,int32_t x,int32_t y){
    if(w->state!=OBS_WINDOW_NORMAL)return 0;
    int32_t right=w->x+(int32_t)w->width+border()*2,bottom=w->y+(int32_t)w->height+title_height()+border();uint8_t direction=0;
    if(x<w->x+RESIZE_HIT)direction|=RESIZE_LEFT;else if(x>=right-RESIZE_HIT)direction|=RESIZE_RIGHT;
    if(y<w->y+RESIZE_HIT)direction|=RESIZE_TOP;else if(y>=bottom-RESIZE_HIT)direction|=RESIZE_BOTTOM;return direction;
}
static void clamp_window(window_slot_t*w){int32_t outer=(int32_t)w->width+border()*2;if(w->x<48-outer)w->x=48-outer;if(w->x>(int32_t)screen_width-48)w->x=(int32_t)screen_width-48;if(w->y<0)w->y=0;if(w->y>(int32_t)screen_height-title_height())w->y=(int32_t)screen_height-title_height();}
static int valid_size(uint32_t width,uint32_t height){if(width<MIN_CLIENT_WIDTH||height<MIN_CLIENT_HEIGHT)return 0;if(width>screen_width-(uint32_t)(border()*2)||height>screen_height-(uint32_t)(title_height()+border()))return 0;uint64_t pixels=(uint64_t)width*height;return pixels<=((uint64_t)screen_width*screen_height)&&pixels<=0x3fffffffULL;}

static void discard_pending(window_slot_t*w){if(!w->pending)return;os_shm_unmap((void*)w->pending_mapping);os_handle_close((uint64_t)w->pending_surface);w->pending=0;w->pending_surface=-1;w->pending_pixels=0;w->pending_since=0;resize_aborts++;}
static int begin_resize(int index,int32_t x,int32_t y,uint32_t width,uint32_t height,uint32_t state,int save_restore){
    if(index<0||index>=MAX_WINDOWS)return-1;
    window_slot_t*w=&windows[index];if(!w->used||w->pending||!valid_size(width,height)||state>OBS_WINDOW_MAXIMIZED||state==OBS_WINDOW_MINIMIZED){rejected_requests++;return-1;}
    int64_t surface=os_surface_create(width,height);if(surface<0)return-1;
    uint64_t primary=CLIENT_MAP_BASE+(uint64_t)index*CLIENT_MAP_STRIDE,pending=PENDING_MAP_BASE+(uint64_t)index*CLIENT_MAP_STRIDE;
    uint64_t mapping=w->mapping_address==pending?primary:pending;const uint32_t*pixels=os_shm_map((uint64_t)surface,(void*)mapping,0);
    if(pixels==(void*)-1){os_handle_close((uint64_t)surface);return-1;}
    uint32_t transaction=next_transaction++;if(!transaction)transaction=next_transaction++;
    w->pending=1;w->pending_transaction=transaction;w->pending_width=width;w->pending_height=height;w->pending_x=x;w->pending_y=y;w->pending_state=state;w->pending_surface=surface;w->pending_mapping=mapping;w->pending_pixels=pixels;w->pending_save_restore=save_restore?1:0;w->pending_since=sys_getticks();
    obs_window_event_t event={0};event.type=OBS_WINDOW_EVENT_RESIZE;event.code=transaction;event.value=(int32_t)state;event.width=width;event.height=height;event.ticks=sys_getticks();
    if(os_ipc_try_send_handle((uint64_t)w->session,&event,sizeof(event),(uint64_t)surface,OS_RIGHT_READ|OS_RIGHT_WRITE|OS_RIGHT_MAP)!=(int64_t)sizeof(event)){discard_pending(w);return-1;}
    return 0;
}
static void commit_resize(window_slot_t*w){
    mark_window_damage(w);int64_t old_surface=w->surface;uint64_t old_mapping=w->mapping_address;
    if(w->pending_save_restore){w->restore_x=w->x;w->restore_y=w->y;w->restore_width=w->width;w->restore_height=w->height;}
    w->surface=w->pending_surface;w->mapping_address=w->pending_mapping;w->pixels=w->pending_pixels;w->width=w->pending_width;w->height=w->pending_height;w->x=w->pending_x;w->y=w->pending_y;w->state=w->pending_state;
    w->pending=0;w->pending_surface=-1;w->pending_pixels=0;w->pending_since=0;if(w->state==OBS_WINDOW_NORMAL){w->restore_x=w->x;w->restore_y=w->y;w->restore_width=w->width;w->restore_height=w->height;}
    os_shm_unmap((void*)old_mapping);os_handle_close((uint64_t)old_surface);resize_commits++;mark_window_damage(w);notify_root(OBS_DESKTOP_EVENT_WINDOW_STATE,w->id,w->state);write("displayd: replacement surface committed\n");
}
static void minimize_window(int index){window_slot_t*w=&windows[index];if(!visible(w)||w->pending)return;mark_window_damage(w);w->pre_minimize_state=w->state;w->state=OBS_WINDOW_MINIMIZED;if(dragged==index)dragged=-1;if(resizing==index)resizing=-1;if(hovered==index){hovered=-1;hovered_control=0;}if(pressed==index){pressed=-1;pressed_control=0;}notify_root(OBS_DESKTOP_EVENT_WINDOW_STATE,w->id,w->state);if(focused==index){focused=-1;w->focused=0;send_event(index,OBS_WINDOW_EVENT_FOCUS,0,0,0,0,sys_getticks());set_focus(top_live_window(),sys_getticks());}write("displayd: window minimized\n");}
static void restore_minimized(int index){window_slot_t*w=&windows[index];if(w->state!=OBS_WINDOW_MINIMIZED)return;w->state=w->pre_minimize_state==OBS_WINDOW_MAXIMIZED?OBS_WINDOW_MAXIMIZED:OBS_WINDOW_NORMAL;mark_window_damage(w);notify_root(OBS_DESKTOP_EVENT_WINDOW_STATE,w->id,w->state);set_focus(index,sys_getticks());write("displayd: minimized window restored\n");}
static void maximize_window(int index){window_slot_t*w=&windows[index];if(w->state==OBS_WINDOW_MINIMIZED){restore_minimized(index);if(w->state==OBS_WINDOW_MAXIMIZED)return;}if(w->state!=OBS_WINDOW_NORMAL||w->pending)return;uint32_t decoration_width=(uint32_t)(border()*2),decoration_height=(uint32_t)(title_height()+border());uint32_t width=work_width>decoration_width?work_width-decoration_width:0,height=work_height>decoration_height?work_height-decoration_height:0;if(begin_resize(index,work_x,work_y,width,height,OBS_WINDOW_MAXIMIZED,1)==0)set_focus(index,sys_getticks());}
static void restore_maximized(int index){window_slot_t*w=&windows[index];if(w->state==OBS_WINDOW_MINIMIZED){restore_minimized(index);return;}if(w->state!=OBS_WINDOW_MAXIMIZED||w->pending)return;if(begin_resize(index,w->restore_x,w->restore_y,w->restore_width,w->restore_height,OBS_WINDOW_NORMAL,0)==0)set_focus(index,sys_getticks());}
static void perform_action(int index,uint32_t action){if(action==OBS_DISPLAY_ACTION_MINIMIZE)minimize_window(index);else if(action==OBS_DISPLAY_ACTION_MAXIMIZE)maximize_window(index);else if(action==OBS_DISPLAY_ACTION_RESTORE){if(windows[index].state==OBS_WINDOW_MINIMIZED)restore_minimized(index);else restore_maximized(index);}else rejected_requests++;}

static void destroy_window(int index,const char*reason){
    if(index<0||index>=MAX_WINDOWS||!windows[index].used)return;
    window_slot_t*w=&windows[index];uint32_t removed_id=w->id;mark_window_damage(w);int was_focused=focused==index;
    if(dragged==index)dragged=-1;
    if(resizing==index)resizing=-1;
    if(hovered==index){hovered=-1;hovered_control=0;}
    if(pressed==index){pressed=-1;pressed_control=0;}
    if(w->pending)discard_pending(w);
    os_shm_unmap((void*)w->mapping_address);os_handle_close((uint64_t)w->surface);os_handle_close((uint64_t)w->session);for(uint32_t i=0;i<sizeof(*w);i++)((uint8_t*)w)[i]=0;if(live_windows)live_windows--;notify_root(OBS_DESKTOP_EVENT_WINDOW_REMOVED,removed_id,0);
    if(was_focused){focused=-1;set_focus(top_live_window(),sys_getticks());}write(reason);
}
static void destroy_root(void){if(root_surface<0)return;os_shm_unmap((void*)root_mapping_address);os_handle_close((uint64_t)root_surface);os_handle_close((uint64_t)root_session);root_surface=root_session=-1;root_pixels=0;root_owner_pid=0;root_id=0;root_mapping_address=0;root_session_missing_since=0;work_x=work_y=0;work_width=screen_width;work_height=screen_height;mark_full_damage();}
static void sweep_dead_clients(void){
    uint64_t now=sys_getticks();for(int i=0;i<MAX_WINDOWS;i++)if(windows[i].used){window_slot_t*w=&windows[i];if(os_process_alive(w->owner_pid)<=0){destroy_window(i,"displayd: orphan window reclaimed\n");continue;}if(w->pending&&now-w->pending_since>=100)discard_pending(w);if(os_handle_has_remote((uint64_t)w->session)>0){w->session_missing_since=0;continue;}if(!w->session_missing_since)w->session_missing_since=now?now:1;else if(now-w->session_missing_since>=2)destroy_window(i,"displayd: orphan window reclaimed\n");}
    if(root_surface>=0){if(os_process_alive(root_owner_pid)<=0)destroy_root();else if(os_handle_has_remote((uint64_t)root_session)>0)root_session_missing_since=0;else if(!root_session_missing_since)root_session_missing_since=now?now:1;else if(now-root_session_missing_since>=2)destroy_root();}
}

static int32_t signed_reserved(uint32_t value){int64_t wide=value;return wide>=0x80000000LL?(int32_t)(wide-0x100000000LL):(int32_t)wide;}
static int valid_input(const obs_input_event_t*e){
    if(e->type==OBS_INPUT_MOUSE_MOVE)return e->code==0&&e->value>=-32768&&e->value<=32767&&signed_reserved(e->reserved)>=-32768&&signed_reserved(e->reserved)<=32767;
    if(e->reserved)return 0;
    if(e->type==OBS_INPUT_KEY)return e->value==0||e->value==1;
    if(e->type==OBS_INPUT_RELATIVE)return(e->code==OBS_INPUT_AXIS_X||e->code==OBS_INPUT_AXIS_Y)&&e->value>=-1024&&e->value<=1024;
    if(e->type==OBS_INPUT_BUTTON)return(e->code>=OBS_INPUT_BUTTON_LEFT&&e->code<=OBS_INPUT_BUTTON_MIDDLE)&&(e->value==0||e->value==1);
    return 0;
}
static void update_resize_candidate(void){
    int32_t dx=cursor_x-resize_start_x,dy=cursor_y-resize_start_y,x=resize_initial_x,y=resize_initial_y;int32_t width=(int32_t)resize_initial_width,height=(int32_t)resize_initial_height;
    if(resize_direction&RESIZE_LEFT){x+=dx;width-=dx;}if(resize_direction&RESIZE_RIGHT)width+=dx;if(resize_direction&RESIZE_TOP){y+=dy;height-=dy;}if(resize_direction&RESIZE_BOTTOM)height+=dy;
    if(width<(int32_t)MIN_CLIENT_WIDTH){if(resize_direction&RESIZE_LEFT)x-=((int32_t)MIN_CLIENT_WIDTH-width);width=MIN_CLIENT_WIDTH;}if(height<(int32_t)MIN_CLIENT_HEIGHT){if(resize_direction&RESIZE_TOP)y-=((int32_t)MIN_CLIENT_HEIGHT-height);height=MIN_CLIENT_HEIGHT;}
    if(width>(int32_t)screen_width-border()*2)width=(int32_t)screen_width-border()*2;
    if(height>(int32_t)screen_height-title_height()-border())height=(int32_t)screen_height-title_height()-border();
    resize_candidate_x=x;resize_candidate_y=y;resize_candidate_width=(uint32_t)width;resize_candidate_height=(uint32_t)height;
}
static void handle_input(const obs_input_event_t*e){
    if(!valid_input(e)){rejected_requests++;return;}
    input_events++;
    if(e->type==OBS_INPUT_RELATIVE||e->type==OBS_INPUT_MOUSE_MOVE){
        int32_t dx=e->type==OBS_INPUT_MOUSE_MOVE?e->value:(e->code==OBS_INPUT_AXIS_X?e->value:0);
        int32_t dy=e->type==OBS_INPUT_MOUSE_MOVE?signed_reserved(e->reserved):(e->code==OBS_INPUT_AXIS_Y?e->value:0);
        mark_damage(cursor_x,cursor_y,9,18);int64_t next_x=(int64_t)cursor_x+dx,next_y=(int64_t)cursor_y+dy;
        cursor_x=next_x<0?0:(next_x>=(int64_t)screen_width?(int32_t)screen_width-1:(int32_t)next_x);cursor_y=next_y<0?0:(next_y>=(int64_t)screen_height?(int32_t)screen_height-1:(int32_t)next_y);
        if(resizing>=0&&left_down&&windows[resizing].used){mark_geometry_damage(resize_candidate_x,resize_candidate_y,resize_candidate_width,resize_candidate_height);update_resize_candidate();mark_geometry_damage(resize_candidate_x,resize_candidate_y,resize_candidate_width,resize_candidate_height);}
        else if(dragged>=0&&left_down&&windows[dragged].used){mark_window_damage(&windows[dragged]);windows[dragged].x=cursor_x-drag_dx;windows[dragged].y=cursor_y-drag_dy;clamp_window(&windows[dragged]);windows[dragged].restore_x=windows[dragged].x;windows[dragged].restore_y=windows[dragged].y;mark_window_damage(&windows[dragged]);}
        update_hover();send_event(-2,OBS_WINDOW_EVENT_MOUSE_MOVE,0,0,cursor_x,cursor_y,e->ticks);mark_damage(cursor_x,cursor_y,9,18);return;
    }
    if(e->type==OBS_INPUT_KEY){if(focused>=0&&visible(&windows[focused]))send_event(focused,OBS_WINDOW_EVENT_KEY,e->code,e->value,0,0,e->ticks);return;}
    int target=hit_test(cursor_x,cursor_y);
    if(e->code==OBS_INPUT_BUTTON_LEFT){left_down=e->value?1:0;if(e->value){
            if(target>=0){set_focus(target,e->ticks);window_slot_t*w=&windows[target];uint32_t control=hit_control(w,cursor_x,cursor_y);uint8_t direction=hit_resize(w,cursor_x,cursor_y);
                if(control){pressed=target;pressed_control=(uint8_t)control;mark_window_damage(w);dragged=resizing=-1;}
                else if(direction&&!w->pending){resizing=target;resize_direction=direction;resize_start_x=cursor_x;resize_start_y=cursor_y;resize_initial_x=resize_candidate_x=w->x;resize_initial_y=resize_candidate_y=w->y;resize_initial_width=resize_candidate_width=w->width;resize_initial_height=resize_candidate_height=w->height;dragged=-1;}
                else if(w->state==OBS_WINDOW_NORMAL&&cursor_y<w->y+title_height()){dragged=target;drag_dx=cursor_x-w->x;drag_dy=cursor_y-w->y;resizing=-1;}
                else send_event(target,OBS_WINDOW_EVENT_MOUSE_BUTTON,e->code,1,cursor_x-w->x-border(),cursor_y-w->y-title_height(),e->ticks);
            }else{set_focus(-1,e->ticks);send_event(-2,OBS_WINDOW_EVENT_MOUSE_BUTTON,e->code,1,cursor_x,cursor_y,e->ticks);}
        }else{
            int control_target=pressed;uint8_t control=pressed_control;pressed=-1;pressed_control=0;
            if(control_target>=0&&windows[control_target].used){window_slot_t*w=&windows[control_target];mark_window_damage(w);dragged=resizing=-1;if(hit_test(cursor_x,cursor_y)==control_target&&hit_control(w,cursor_x,cursor_y)==control){if(control==3)send_event(control_target,OBS_WINDOW_EVENT_CLOSE,0,1,0,0,e->ticks);else if(control==2){if(w->state==OBS_WINDOW_MAXIMIZED)restore_maximized(control_target);else maximize_window(control_target);}else if(control==1)minimize_window(control_target);}return;}
            int resize_target=resizing;int32_t rx=resize_candidate_x,ry=resize_candidate_y;uint32_t rw=resize_candidate_width,rh=resize_candidate_height;dragged=-1;resizing=-1;
            if(resize_target>=0&&windows[resize_target].used){mark_geometry_damage(rx,ry,rw,rh);begin_resize(resize_target,rx,ry,rw,rh,OBS_WINDOW_NORMAL,0);}
            else if(target>=0){window_slot_t*w=&windows[target];send_event(target,OBS_WINDOW_EVENT_MOUSE_BUTTON,e->code,0,cursor_x-w->x-border(),cursor_y-w->y-title_height(),e->ticks);}else send_event(-2,OBS_WINDOW_EVENT_MOUSE_BUTTON,e->code,0,cursor_x,cursor_y,e->ticks);
        }return;
    }
    if(target>=0){window_slot_t*w=&windows[target];send_event(target,OBS_WINDOW_EVENT_MOUSE_BUTTON,e->code,e->value,cursor_x-w->x-border(),cursor_y-w->y-title_height(),e->ticks);}else send_event(-2,OBS_WINDOW_EVENT_MOUSE_BUTTON,e->code,e->value,cursor_x,cursor_y,e->ticks);
}

static void reject_create(int64_t session){obs_display_create_response_t response={-1,0,0,0};if(session>=0){os_ipc_try_send((uint64_t)session,&response,sizeof(response));os_handle_close((uint64_t)session);}rejected_requests++;}
static uint32_t allocate_window_id(int index){uint32_t generation=(slot_generations[index]+1U)&0x07ffffffU;if(!generation)generation=1;slot_generations[index]=generation;return(generation<<5)|(uint32_t)(index+1);}
static void create_client(const obs_display_create_request_t*request,int64_t session,uint64_t owner_pid){
    int root=request->operation==OBS_DISPLAY_CREATE_ROOT,index=-1;if(session<0||!owner_pid||request->flags||!request->width||!request->height||(root?(root_surface>=0||request->width!=screen_width||request->height!=screen_height):(!valid_size(request->width,request->height)||request->width>1024||request->height>768))){reject_create(session);return;}
    if(!root){for(int i=0;i<MAX_WINDOWS;i++)if(!windows[i].used){index=i;break;}if(index<0){reject_create(session);return;}}
    int64_t surface=os_surface_create(request->width,request->height);if(surface<0){reject_create(session);return;}uint64_t address=root?ROOT_MAP:CLIENT_MAP_BASE+(uint64_t)index*CLIENT_MAP_STRIDE;const uint32_t*pixels=os_shm_map((uint64_t)surface,(void*)address,0);if(pixels==(void*)-1){os_handle_close((uint64_t)surface);reject_create(session);return;}uint32_t id=root?ROOT_WINDOW_ID:allocate_window_id(index);
    if(root){root_surface=surface;root_session=session;root_pixels=pixels;root_id=id;root_owner_pid=owner_pid;root_mapping_address=address;}
    else{window_slot_t*w=&windows[index];*w=(window_slot_t){.used=1,.id=id,.width=request->width,.height=request->height,.z=next_z++,.generation=slot_generations[index],.state=OBS_WINDOW_NORMAL,.pre_minimize_state=OBS_WINDOW_NORMAL,.x=180+index*24,.y=100+index*20,.restore_x=180+index*24,.restore_y=100+index*20,.restore_width=request->width,.restore_height=request->height,.surface=surface,.session=session,.owner_pid=owner_pid,.mapping_address=address,.pixels=pixels,.pending_surface=-1};for(uint32_t i=0;i<sizeof(w->title)-1&&request->title[i];i++){unsigned char ch=(unsigned char)request->title[i];w->title[i]=ch>=32&&ch<=126?(char)ch:'?';}if(!w->title[0]){w->title[0]='A';w->title[1]='p';w->title[2]='p';}live_windows++;}
    obs_display_create_response_t response={0,id,request->width,request->height};if(os_ipc_send_handle((uint64_t)session,&response,sizeof(response),(uint64_t)surface,OS_RIGHT_READ|OS_RIGHT_WRITE|OS_RIGHT_MAP)!=(int64_t)sizeof(response)){if(root)destroy_root();else destroy_window(index,"displayd: failed client creation reclaimed\n");return;}
    if(root){mark_full_damage();for(int i=0;i<MAX_WINDOWS;i++)if(windows[i].used){notify_root(OBS_DESKTOP_EVENT_WINDOW_ADDED,windows[i].id,windows[i].state);if(windows[i].focused)notify_root(OBS_DESKTOP_EVENT_WINDOW_FOCUSED,windows[i].id,windows[i].state);}write("displayd: desktop root surface created\n");}
    else{notify_root(OBS_DESKTOP_EVENT_WINDOW_ADDED,id,OBS_WINDOW_NORMAL);set_focus(index,sys_getticks());mark_window_damage(&windows[index]);write("displayd: application window created\n");}
}
static void query_stats(int64_t reply){obs_display_stats_response_t response={0,live_windows,damage_events,full_damage_fallbacks,rejected_requests,resize_commits,resize_aborts,cursor_x,cursor_y,input_events};if(reply>=0){os_ipc_try_send((uint64_t)reply,&response,sizeof(response));os_handle_close((uint64_t)reply);}}
static void reply_close(int64_t reply,int32_t status){if(reply>=0){obs_display_close_response_t response={status};os_ipc_try_send((uint64_t)reply,&response,sizeof(response));os_handle_close((uint64_t)reply);}}

static void dispatch(const uint8_t*message,int64_t received,const os_ipc_message_info_t*info){
    if(received<4){if(info->attached>=0)os_handle_close((uint64_t)info->attached);rejected_requests++;return;}uint32_t operation=*(const uint32_t*)message;
    if(operation==OBS_DISPLAY_CREATE_WINDOW||operation==OBS_DISPLAY_CREATE_ROOT){if(received==(int64_t)sizeof(obs_display_create_request_t)&&info->attached>=0)create_client((const obs_display_create_request_t*)message,info->attached,info->sender_pid);else reject_create(info->attached);return;}
    if(operation==OBS_DISPLAY_QUERY_STATS){if(received==(int64_t)sizeof(obs_display_query_request_t)&&info->attached>=0)query_stats(info->attached);else{if(info->attached>=0)os_handle_close((uint64_t)info->attached);rejected_requests++;}return;}
    if(info->attached>=0&&operation!=OBS_DISPLAY_CLOSE_WINDOW){reject_create(info->attached);return;}
    if(operation==OBS_DISPLAY_PRESENT_WINDOW&&received==(int64_t)sizeof(obs_display_window_request_t)){uint32_t id=((const obs_display_window_request_t*)message)->window_id;if(id==root_id&&info->sender_pid==root_owner_pid)mark_full_damage();else{int index=find_window(id,info->sender_pid);if(index<0){rejected_requests++;return;}window_slot_t*w=&windows[index];if(visible(w))mark_damage(w->x+border(),w->y+title_height(),(int32_t)w->width,(int32_t)w->height);}return;}
    if(operation==OBS_DISPLAY_CLOSE_WINDOW&&received==(int64_t)sizeof(obs_display_window_request_t)){uint32_t id=((const obs_display_window_request_t*)message)->window_id;int status=0;if(id==root_id&&info->sender_pid==root_owner_pid)destroy_root();else{int index=find_window(id,info->sender_pid);if(index<0){rejected_requests++;status=-1;}else destroy_window(index,"displayd: client window closed and reclaimed\n");}reply_close(info->attached,status);return;}
    if(operation==OBS_DISPLAY_RESIZE_WINDOW&&received==(int64_t)sizeof(obs_display_resize_request_t)){const obs_display_resize_request_t*r=(const obs_display_resize_request_t*)message;int index=find_window(r->window_id,info->sender_pid);if(index<0||windows[index].state!=OBS_WINDOW_NORMAL||begin_resize(index,windows[index].x,windows[index].y,r->width,r->height,OBS_WINDOW_NORMAL,0)<0)rejected_requests++;return;}
    if(operation==OBS_DISPLAY_RESIZE_REPLY&&received==(int64_t)sizeof(obs_display_resize_reply_t)){const obs_display_resize_reply_t*r=(const obs_display_resize_reply_t*)message;int index=find_window(r->window_id,info->sender_pid);if(index<0||!windows[index].pending||windows[index].pending_transaction!=r->transaction){rejected_requests++;return;}if(r->result==OBS_DISPLAY_RESIZE_ACCEPT)commit_resize(&windows[index]);else if(r->result==OBS_DISPLAY_RESIZE_ABORT)discard_pending(&windows[index]);else rejected_requests++;return;}
    if(operation==OBS_DISPLAY_WINDOW_ACTION&&received==(int64_t)sizeof(obs_display_action_request_t)){const obs_display_action_request_t*r=(const obs_display_action_request_t*)message;int index=find_window(r->window_id,info->sender_pid);if(index<0||r->reserved){rejected_requests++;return;}perform_action(index,r->action);return;}
    if(operation==OBS_DISPLAY_CONFIGURE_ROOT&&received==(int64_t)sizeof(obs_display_root_config_request_t)){const obs_display_root_config_request_t*r=(const obs_display_root_config_request_t*)message;uint64_t right=(uint64_t)(r->x<0?0:r->x)+r->width,bottom=(uint64_t)(r->y<0?0:r->y)+r->height;if(r->root_id!=root_id||info->sender_pid!=root_owner_pid||r->x<0||r->y<0||!r->width||!r->height||right>screen_width||bottom>screen_height){rejected_requests++;return;}work_x=r->x;work_y=r->y;work_width=r->width;work_height=r->height;for(int i=0;i<MAX_WINDOWS;i++)if(windows[i].used&&windows[i].state==OBS_WINDOW_MAXIMIZED&&!windows[i].pending){uint32_t dw=(uint32_t)(border()*2),dh=(uint32_t)(title_height()+border());begin_resize(i,work_x,work_y,work_width>dw?work_width-dw:0,work_height>dh?work_height-dh:0,OBS_WINDOW_MAXIMIZED,0);}mark_full_damage();return;}
    if(operation==OBS_DISPLAY_MANAGE_WINDOW&&received==(int64_t)sizeof(obs_display_manage_request_t)){const obs_display_manage_request_t*r=(const obs_display_manage_request_t*)message;int index=id_index(r->window_id);if(r->root_id!=root_id||info->sender_pid!=root_owner_pid||r->action!=OBS_DISPLAY_MANAGE_FOCUS_RESTORE||index<0){rejected_requests++;return;}if(windows[index].state==OBS_WINDOW_MINIMIZED)restore_minimized(index);else set_focus(index,sys_getticks());return;}
    if(operation==OBS_DISPLAY_FORWARD_INPUT&&received==(int64_t)sizeof(obs_display_input_request_t)){handle_input(&((const obs_display_input_request_t*)message)->event);return;}rejected_requests++;
}

int obsidia_main(void){
    active_theme=*obs_theme_get_default();theme=&active_theme;connect_settings();fb_info_t info;sys_fbinfo(&info);screen_width=(uint32_t)info.width;screen_height=(uint32_t)info.height;if(!screen_width||!screen_height||screen_width>4096||screen_height>4096)return 1;work_width=screen_width;work_height=screen_height;
    output_capability=os_handle_find(OS_OBJECT_DISPLAY_OUTPUT);if(output_capability<0)return 2;output_surface=os_surface_create(screen_width,screen_height);if(output_surface<0)return 3;output_pixels=os_shm_map((uint64_t)output_surface,(void*)OUTPUT_MAP,1);if(output_pixels==(void*)-1)return 4;
    obs_canvas_init(&output_canvas,output_pixels,screen_width,screen_height,screen_width);
    int64_t endpoint=os_ipc_create();if(endpoint<0||os_service_register_restricted("display",(uint64_t)endpoint,OS_RIGHT_WRITE|OS_RIGHT_DUP)<0)return 5;cursor_x=(int32_t)screen_width/2;cursor_y=(int32_t)screen_height/2;mark_full_damage();repaint();write("displayd: stateful compositor ready\n");
    for(;;){
        poll_settings();
        int processed=0;
        for(uint32_t burst=0;burst<64;burst++){
            uint8_t message[64];os_ipc_message_info_t message_info={-1,0};int64_t received=os_ipc_recv_ex((uint64_t)endpoint,message,sizeof(message),&message_info,1);
            if(received==-2)break;
            if(received<0){sys_yield();break;}
            dispatch(message,received,&message_info);processed=1;
        }
        sweep_dead_clients();repaint();
        if(!processed)sys_sleep(1);else sys_yield();
    }
}
