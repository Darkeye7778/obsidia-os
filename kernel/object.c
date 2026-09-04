#include "object.h"
#include "task.h"
void object_retain(kobject_t* o){if(o)o->refs++;}
void object_release(kobject_t* o){if(o&&o->refs&&!--o->refs&&o->destroy)o->destroy(o);}
static uint64_t encode(int i,uint32_t g){return ((uint64_t)g<<32)|(uint32_t)(i+1);}
static int decode(uint64_t h){uint32_t n=(uint32_t)h;return n&&n<=MAX_HANDLES?(int)n-1:-1;}
int64_t handle_install(process_t* p,kobject_t* o,uint32_t rights,int inherit){if(!p||!o)return -1;for(int i=0;i<MAX_HANDLES;i++)if(!p->handles[i].object){uint32_t g=p->handles[i].generation+1;if(!g)g=1;p->handles[i]=(handle_entry_t){o,g,rights,inherit?1:0};object_retain(o);return (int64_t)encode(i,g);}return -1;}
kobject_t* handle_get(process_t* p,uint64_t h,uint32_t type,uint32_t rights){int i=decode(h);if(!p||i<0)return 0;handle_entry_t* e=&p->handles[i];if(!e->object||e->generation!=(uint32_t)(h>>32)||(type&&e->object->type!=type)||(e->rights&rights)!=rights)return 0;return e->object;}
int handle_close(process_t* p,uint64_t h){int i=decode(h);if(!p||i<0||p->handles[i].generation!=(uint32_t)(h>>32)||!p->handles[i].object)return -1;kobject_t* o=p->handles[i].object;p->handles[i].object=0;p->handles[i].rights=0;p->handles[i].inheritable=0;object_release(o);return 0;}
void handles_inherit(process_t* c,process_t* p){for(int i=0;i<MAX_HANDLES;i++)if(p->handles[i].object&&p->handles[i].inheritable){c->handles[i]=p->handles[i];object_retain(c->handles[i].object);}}
void handles_close_all(process_t* p){for(int i=0;i<MAX_HANDLES;i++)if(p->handles[i].object){object_release(p->handles[i].object);p->handles[i].object=0;}}
int64_t handle_find(process_t* p,uint32_t type){for(int i=0;i<MAX_HANDLES;i++)if(p->handles[i].object&&p->handles[i].object->type==type)return (int64_t)encode(i,p->handles[i].generation);return -1;}
