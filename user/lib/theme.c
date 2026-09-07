#include <obsidia/theme.h>

static const obs_theme_t default_theme={
    "Obsidia Default",
    {0x000b1019,0x000f1722,0x00e8edf5,0x00141b26,0x002a3748,0x00202b3b,0x002a3a50,0x00101a28,
     0x0018202c,0x005f91d8,0x00374558,0x0004080e,0x00243f68,0x00242d3a,0x00f5f8fc,0x00b7c2d0,
     0x002c394b,0x003d526c,0x001b2635,0x00bd4d59,0x00dc5967,0x00eef2f7,0x00aab5c3,0x006d7784,
     0x006fa8f2,0x005cd1c5,0x00345578,0x00ffffff},
    {2,30,18,5,7,4,8,14,40,2,6,132,32}
};
static const obs_theme_t alternate_theme={
    "Obsidia Alternate Test",
    {0x0021182b,0x00312642,0x00fff4dc,0x00342649,0x00664f76,0x00433259,0x005b4275,0x00271b39,
     0x002c2039,0x00e0a94d,0x00705484,0x00120c18,0x00603c78,0x00463159,0x00fff8e8,0x00d8c5e5,
     0x00523c67,0x0071538d,0x00332645,0x00c94a61,0x00ec667b,0x00fff7e5,0x00dbc9df,0x009b86a6,
     0x00e0a94d,0x0088c96b,0x00664f76,0x00fff8e8},
    {2,30,18,5,7,4,8,14,40,2,6,132,32}
};
const obs_theme_t *obs_theme_get(obs_theme_id_t id){return id==OBS_THEME_ALTERNATE?&alternate_theme:&default_theme;}
const obs_theme_t *obs_theme_get_default(void){
#ifdef OBSIDIA_THEME_ALTERNATE
    return &alternate_theme;
#else
    return &default_theme;
#endif
}
