#include <obsidia/gfx.h>

/* Project-owned 5x7 UI font. Rows use their low five bits, left to right. */
static const uint8_t font[95][7]={
 ['!'-32]={4,4,4,4,4,0,4},['"'-32]={10,10,10,0,0,0,0},['#'-32]={10,31,10,10,31,10,0},
 ['$'-32]={4,15,20,14,5,30,4},['%'-32]={24,25,2,4,8,19,3},['&'-32]={12,18,20,8,21,18,13},
 ['\''-32]={4,4,8,0,0,0,0},['('-32]={2,4,8,8,8,4,2},[')'-32]={8,4,2,2,2,4,8},
 ['*'-32]={0,21,14,31,14,21,0},['+'-32]={0,4,4,31,4,4,0},[','-32]={0,0,0,0,4,4,8},
 ['-'-32]={0,0,0,31,0,0,0},['.'-32]={0,0,0,0,0,12,12},['/'-32]={1,2,2,4,8,8,16},
 ['0'-32]={14,17,19,21,25,17,14},['1'-32]={4,12,4,4,4,4,14},['2'-32]={14,17,1,2,4,8,31},
 ['3'-32]={30,1,1,14,1,1,30},['4'-32]={2,6,10,18,31,2,2},['5'-32]={31,16,16,30,1,1,30},
 ['6'-32]={14,16,16,30,17,17,14},['7'-32]={31,1,2,4,8,8,8},['8'-32]={14,17,17,14,17,17,14},
 ['9'-32]={14,17,17,15,1,1,14},[':'-32]={0,12,12,0,12,12,0},[';'-32]={0,12,12,0,4,4,8},
 ['<'-32]={2,4,8,16,8,4,2},['='-32]={0,0,31,0,31,0,0},['>'-32]={8,4,2,1,2,4,8},
 ['?'-32]={14,17,1,2,4,0,4},['@'-32]={14,17,1,13,21,21,14},
 ['A'-32]={14,17,17,31,17,17,17},['B'-32]={30,17,17,30,17,17,30},['C'-32]={14,17,16,16,16,17,14},
 ['D'-32]={30,17,17,17,17,17,30},['E'-32]={31,16,16,30,16,16,31},['F'-32]={31,16,16,30,16,16,16},
 ['G'-32]={14,17,16,23,17,17,15},['H'-32]={17,17,17,31,17,17,17},['I'-32]={14,4,4,4,4,4,14},
 ['J'-32]={7,2,2,2,2,18,12},['K'-32]={17,18,20,24,20,18,17},['L'-32]={16,16,16,16,16,16,31},
 ['M'-32]={17,27,21,21,17,17,17},['N'-32]={17,25,21,19,17,17,17},['O'-32]={14,17,17,17,17,17,14},
 ['P'-32]={30,17,17,30,16,16,16},['Q'-32]={14,17,17,17,21,18,13},['R'-32]={30,17,17,30,20,18,17},
 ['S'-32]={15,16,16,14,1,1,30},['T'-32]={31,4,4,4,4,4,4},['U'-32]={17,17,17,17,17,17,14},
 ['V'-32]={17,17,17,17,17,10,4},['W'-32]={17,17,17,21,21,21,10},['X'-32]={17,17,10,4,10,17,17},
 ['Y'-32]={17,17,10,4,4,4,4},['Z'-32]={31,1,2,4,8,16,31},
 ['['-32]={14,8,8,8,8,8,14},['\\'-32]={16,8,8,4,2,2,1},[']'-32]={14,2,2,2,2,2,14},
 ['^'-32]={4,10,17,0,0,0,0},['_'-32]={0,0,0,0,0,0,31},['`'-32]={8,4,2,0,0,0,0},
 ['a'-32]={0,0,14,1,15,17,15},['b'-32]={16,16,30,17,17,17,30},['c'-32]={0,0,14,17,16,17,14},
 ['d'-32]={1,1,15,17,17,17,15},['e'-32]={0,0,14,17,31,16,14},['f'-32]={6,8,8,30,8,8,8},
 ['g'-32]={0,15,17,17,15,1,14},['h'-32]={16,16,30,17,17,17,17},['i'-32]={4,0,12,4,4,4,14},
 ['j'-32]={2,0,6,2,2,18,12},['k'-32]={16,16,18,20,24,20,18},['l'-32]={12,4,4,4,4,4,14},
 ['m'-32]={0,0,26,21,21,17,17},['n'-32]={0,0,30,17,17,17,17},['o'-32]={0,0,14,17,17,17,14},
 ['p'-32]={0,0,30,17,30,16,16},['q'-32]={0,0,15,17,15,1,1},['r'-32]={0,0,22,25,16,16,16},
 ['s'-32]={0,0,15,16,14,1,30},['t'-32]={8,8,30,8,8,9,6},['u'-32]={0,0,17,17,17,19,13},
 ['v'-32]={0,0,17,17,17,10,4},['w'-32]={0,0,17,17,21,21,10},['x'-32]={0,0,17,10,4,10,17},
 ['y'-32]={0,0,17,17,15,1,14},['z'-32]={0,0,31,2,4,8,31},
 ['{'-32]={2,4,4,8,4,4,2},['|'-32]={4,4,4,4,4,4,4},['}'-32]={8,4,4,2,4,4,8},['~'-32]={0,0,9,22,0,0,0}
};

static int inside(const obs_canvas_t*c,int32_t x,int32_t y){
    if(!c||!c->pixels||x<0||y<0||x>=(int32_t)c->width||y>=(int32_t)c->height)return 0;
    int64_t right=(int64_t)c->clip_x+c->clip_width,bottom=(int64_t)c->clip_y+c->clip_height;
    return x>=c->clip_x&&y>=c->clip_y&&(int64_t)x<right&&(int64_t)y<bottom;
}
void obs_canvas_init(obs_canvas_t*c,uint32_t*p,uint32_t w,uint32_t h,uint32_t stride){if(!c)return;*c=(obs_canvas_t){p,w,h,stride,0,0,w,h};}
void obs_canvas_reset_clip(obs_canvas_t*c){if(c){c->clip_x=c->clip_y=0;c->clip_width=c->width;c->clip_height=c->height;}}
void obs_canvas_set_clip(obs_canvas_t*c,int32_t x,int32_t y,uint32_t w,uint32_t h){
    if(!c)return;
    int64_t left=x<0?0:x,top=y<0?0:y,right=(int64_t)x+w,bottom=(int64_t)y+h;
    if(right>(int64_t)c->width)right=c->width;
    if(bottom>(int64_t)c->height)bottom=c->height;
    if(right<left)right=left;
    if(bottom<top)bottom=top;
    c->clip_x=(int32_t)left;c->clip_y=(int32_t)top;c->clip_width=(uint32_t)(right-left);c->clip_height=(uint32_t)(bottom-top);
}
void obs_fill_rect(obs_canvas_t*c,int32_t x,int32_t y,int32_t w,int32_t h,uint32_t color){
    if(!c||!c->pixels||w<=0||h<=0)return;
    int64_t l=x,t=y,r=(int64_t)x+w,b=(int64_t)y+h,cl=c->clip_x,ct=c->clip_y,cr=cl+c->clip_width,cb=ct+c->clip_height;
    if(l<cl)l=cl;
    if(t<ct)t=ct;
    if(r>cr)r=cr;
    if(b>cb)b=cb;
    if(l<0)l=0;
    if(t<0)t=0;
    if(r>c->width)r=c->width;
    if(b>c->height)b=c->height;
    if(r<=l||b<=t)return;
    for(int32_t py=(int32_t)t;py<(int32_t)b;py++)for(int32_t px=(int32_t)l;px<(int32_t)r;px++)c->pixels[(uint32_t)py*c->stride+(uint32_t)px]=color;
}
void obs_hline(obs_canvas_t*c,int32_t x,int32_t y,int32_t w,uint32_t color){obs_fill_rect(c,x,y,w,1,color);}
void obs_vline(obs_canvas_t*c,int32_t x,int32_t y,int32_t h,uint32_t color){obs_fill_rect(c,x,y,1,h,color);}
void obs_stroke_rect(obs_canvas_t*c,int32_t x,int32_t y,int32_t w,int32_t h,uint32_t n,uint32_t color){if(!n||w<=0||h<=0)return;if(n>(uint32_t)w/2)n=(uint32_t)w/2;if(n>(uint32_t)h/2)n=(uint32_t)h/2;obs_fill_rect(c,x,y,w,(int32_t)n,color);obs_fill_rect(c,x,y+h-(int32_t)n,w,(int32_t)n,color);obs_fill_rect(c,x,y+(int32_t)n,(int32_t)n,h-(int32_t)n*2,color);obs_fill_rect(c,x+w-(int32_t)n,y+(int32_t)n,(int32_t)n,h-(int32_t)n*2,color);}
void obs_blit(obs_canvas_t*c,int32_t x,int32_t y,uint32_t w,uint32_t h,const uint32_t*s,uint32_t stride){if(!c||!s||!stride)return;for(uint32_t py=0;py<h;py++)for(uint32_t px=0;px<w;px++)if(inside(c,x+(int32_t)px,y+(int32_t)py))c->pixels[(uint32_t)(y+(int32_t)py)*c->stride+(uint32_t)(x+(int32_t)px)]=s[py*stride+px];}
uint32_t obs_glyph_width(uint32_t scale){return scale?5*scale:0;}
uint32_t obs_glyph_height(uint32_t scale){return scale?7*scale:0;}
uint32_t obs_text_width(const char*text,uint32_t scale){if(!text||!scale)return 0;uint64_t count=0;while(text[count]&&count<4096)count++;if(!count)return 0;uint64_t width=count*(6ULL*scale)-scale;return width>0xffffffffULL?0xffffffffU:(uint32_t)width;}
void obs_draw_glyph(obs_canvas_t*c,int32_t x,int32_t y,unsigned char ch,uint32_t color,uint32_t scale){if(!scale)return;if(ch<32||ch>126)ch='?';const uint8_t*rows=font[ch-32];for(uint32_t row=0;row<7;row++)for(uint32_t col=0;col<5;col++)if(rows[row]&(1U<<(4-col)))obs_fill_rect(c,x+(int32_t)(col*scale),y+(int32_t)(row*scale),(int32_t)scale,(int32_t)scale,color);}
void obs_draw_text(obs_canvas_t*c,int32_t x,int32_t y,const char*text,uint32_t color,uint32_t scale){if(!text||!scale)return;for(uint32_t i=0;text[i]&&i<4096;i++){obs_draw_glyph(c,x,y,(unsigned char)text[i],color,scale);x+=(int32_t)(6*scale);}}
void obs_draw_text_clipped(obs_canvas_t*c,int32_t x,int32_t y,uint32_t max_width,const char*text,uint32_t color,uint32_t scale){if(!c||!max_width)return;obs_canvas_t saved=*c;int64_t right=(int64_t)x+max_width,bottom=(int64_t)y+obs_glyph_height(scale);int32_t l=x>c->clip_x?x:c->clip_x,t=y>c->clip_y?y:c->clip_y;int64_t old_r=(int64_t)c->clip_x+c->clip_width,old_b=(int64_t)c->clip_y+c->clip_height;if(right>old_r)right=old_r;if(bottom>old_b)bottom=old_b;obs_canvas_set_clip(c,l,t,right>l?(uint32_t)(right-l):0,bottom>t?(uint32_t)(bottom-t):0);obs_draw_text(c,x,y,text,color,scale);*c=saved;}
void obs_draw_icon(obs_canvas_t*c,int32_t x,int32_t y,uint32_t size,obs_icon_id_t icon,uint32_t fg,uint32_t accent){
    if(!c||size<12)return;
    int32_t s=(int32_t)size;
    if(icon==OBS_ICON_SYSTEM){int32_t cx=x+s/2,cy=y+s/2;for(int32_t row=-s/2;row<=s/2;row++){int32_t half=(s/2-(row<0?-row:row))/2;obs_hline(c,cx-half,cy+row,half*2+1,accent);}obs_stroke_rect(c,x+s/3,y+s/3,s/3,s/3,1,fg);}
    else if(icon==OBS_ICON_SETTINGS){obs_fill_rect(c,x+s/4,y+s/4,s/2,s/2,fg);obs_fill_rect(c,x+s/3,y+s/3,s/3,s/3,accent);obs_fill_rect(c,x+s/2-2,y+s/8,4,s*3/4,fg);obs_fill_rect(c,x+s/8,y+s/2-2,s*3/4,4,fg);obs_fill_rect(c,x+s/2-3,y+s/2-3,6,6,accent);}
    else{obs_fill_rect(c,x+s/8,y+s/6,s*3/4,s*2/3,fg);obs_fill_rect(c,x+s/8+2,y+s/6+3,s*3/4-4,s*2/3-5,accent);obs_hline(c,x+s/8+2,y+s/6+6,s*3/4-4,fg);if(icon==OBS_ICON_APPLICATION)obs_fill_rect(c,x+s/3,y+s/3,s/3,s/3,fg);}
}
