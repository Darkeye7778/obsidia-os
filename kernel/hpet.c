#include "hpet.h"
#include "acpi.h"
#include "paging.h"
#include <stdint.h>

#define HPET_CAPABILITIES 0x000
#define HPET_CONFIGURATION 0x010
#define HPET_COUNTER 0x0f0
#define HPET_ENABLE 1ULL
#define FEMTOSECONDS_PER_NANOSECOND 1000000ULL

static volatile uint8_t*registers;
static uint64_t period_femtoseconds;
static uint64_t counter_high;
static uint32_t last_counter_low;
static uint8_t available,counter_is_64_bit;

extern void serial_write(const char*);

static uint64_t read64(uint32_t offset){
    return *(volatile uint64_t*)(registers+offset);
}
static void write64(uint32_t offset,uint64_t value){
    *(volatile uint64_t*)(registers+offset)=value;
}

int hpet_init(void){
    const acpi_platform_info_t*platform=acpi_platform_info();
    if(!platform||!platform->has_hpet)return-1;
    registers=paging_map_mmio(platform->hpet_address,1024);
    if(!registers)return-1;
    uint64_t capabilities=read64(HPET_CAPABILITIES);
    period_femtoseconds=capabilities>>32;
    counter_is_64_bit=(capabilities&(1ULL<<13))!=0;
    /* ACPI requires a nonzero period no greater than 100 ns. */
    if(!period_femtoseconds||period_femtoseconds>100000000ULL){registers=0;return-1;}
    uint64_t configuration=read64(HPET_CONFIGURATION);
    write64(HPET_CONFIGURATION,configuration&~HPET_ENABLE);
    write64(HPET_COUNTER,0);
    write64(HPET_CONFIGURATION,(configuration&~2ULL)|HPET_ENABLE);
    if(read64(HPET_CONFIGURATION)&HPET_ENABLE){
        available=1;serial_write("HPET: high-resolution monotonic clock ready\n");return 0;
    }
    registers=0;return-1;
}

int hpet_is_available(void){return available!=0;}

uint64_t hpet_monotonic_ns(void){
    if(!available)return 0;
    uint64_t counter;
    if(counter_is_64_bit)counter=read64(HPET_COUNTER);
    else{
        uint32_t low=*(volatile uint32_t*)(registers+HPET_COUNTER);
        if(low<last_counter_low)counter_high+=1ULL<<32;
        last_counter_low=low;counter=counter_high|low;
    }
    /* Split the product so ordinary 64-bit code needs no compiler runtime. */
    return(counter/FEMTOSECONDS_PER_NANOSECOND)*period_femtoseconds+
           ((counter%FEMTOSECONDS_PER_NANOSECOND)*period_femtoseconds)/FEMTOSECONDS_PER_NANOSECOND;
}

uint64_t hpet_resolution_ns(void){
    return available?(period_femtoseconds+FEMTOSECONDS_PER_NANOSECOND-1)/FEMTOSECONDS_PER_NANOSECOND:0;
}
