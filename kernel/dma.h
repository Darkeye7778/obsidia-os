#pragma once
#include <stdint.h>

typedef struct {
    void* virtual_address;
    uint64_t physical_address;
    uint64_t size;
    uint64_t pages;
} dma_buffer_t;

/* Contiguous, page-aligned, zeroed coherent storage. The current x86_64
   backend uses the kernel identity map; max_address constrains legacy devices. */
int dma_allocate(uint64_t size,uint64_t max_address,dma_buffer_t*buffer);
void dma_release(dma_buffer_t*buffer);
int dma_self_test(void);

