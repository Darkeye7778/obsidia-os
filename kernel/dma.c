#include "dma.h"
#include "memory/memory.h"

int dma_allocate(uint64_t size,uint64_t max_address,dma_buffer_t*buffer){
    if(!buffer||!size||size>4ULL*1024*1024)return-1;uint64_t pages=(size+4095)/4096;void*memory=pmm_alloc_pages(pages);if(!memory)return-1;uint64_t physical=(uint64_t)memory;
    if(physical>max_address||pages*4096-1>max_address-physical){pmm_free_pages(memory,pages);return-1;}
    for(uint64_t i=0;i<pages*4096;i++)((uint8_t*)memory)[i]=0;*buffer=(dma_buffer_t){memory,physical,pages*4096,pages};return 0;
}
void dma_release(dma_buffer_t*buffer){if(!buffer||!buffer->virtual_address)return;pmm_free_pages(buffer->virtual_address,buffer->pages);*buffer=(dma_buffer_t){0};}
int dma_self_test(void){uint64_t before=memory_get_free_pages();dma_buffer_t buffer={0};if(dma_allocate(8193,0xffffffffULL,&buffer)||!buffer.virtual_address||(buffer.physical_address&4095)||buffer.pages!=3)return-1;((uint8_t*)buffer.virtual_address)[8192]=0xa5;dma_release(&buffer);return memory_get_free_pages()==before?0:-1;}
