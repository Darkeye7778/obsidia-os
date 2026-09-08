#include "apic.h"
#include "acpi.h"
#include "paging.h"
#include "idt.h"
#include <stdint.h>

#define APIC_MSR_BASE 0x1bU
#define LAPIC_ID 0x20U
#define LAPIC_TPR 0x80U
#define LAPIC_EOI 0xb0U
#define LAPIC_SVR 0xf0U
#define LAPIC_LVT_TIMER 0x320U
#define LAPIC_LVT_THERMAL 0x330U
#define LAPIC_LVT_PERFORMANCE 0x340U
#define LAPIC_LVT_LINT0 0x350U
#define LAPIC_LVT_LINT1 0x360U
#define LAPIC_LVT_ERROR 0x370U
#define LAPIC_MASKED (1U<<16)

typedef struct {
    volatile uint32_t*registers;
    uint32_t gsi_base;
    uint32_t redirection_count;
} ioapic_runtime_t;

static volatile uint32_t*lapic;
static ioapic_runtime_t ioapics[ACPI_MAX_IOAPICS];
static uint8_t ioapic_count,bsp_apic_id,active;

extern void serial_write(const char*);

static uint64_t read_msr(uint32_t msr){uint32_t low,high;__asm__ volatile("rdmsr":"=a"(low),"=d"(high):"c"(msr));return((uint64_t)high<<32)|low;}
static void write_msr(uint32_t msr,uint64_t value){__asm__ volatile("wrmsr"::"c"(msr),"a"((uint32_t)value),"d"((uint32_t)(value>>32)));}
static uint32_t lapic_read(uint32_t offset){return lapic[offset/4];}
static void lapic_write(uint32_t offset,uint32_t value){lapic[offset/4]=value;(void)lapic[LAPIC_ID/4];}
static uint32_t ioapic_read(ioapic_runtime_t*io,uint8_t reg){io->registers[0]=reg;return io->registers[4];}
static void ioapic_write(ioapic_runtime_t*io,uint8_t reg,uint32_t value){io->registers[0]=reg;io->registers[4]=value;}

static ioapic_runtime_t*ioapic_for_gsi(uint32_t gsi){
    for(uint8_t i=0;i<ioapic_count;i++)if(gsi>=ioapics[i].gsi_base&&gsi-ioapics[i].gsi_base<ioapics[i].redirection_count)return&ioapics[i];
    return 0;
}

int apic_init(void){
    const acpi_platform_info_t*platform=acpi_platform_info();uint32_t eax,ebx,ecx,edx;
    __asm__ volatile("cpuid":"=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(1),"c"(0));
    if(!platform||!platform->has_madt||!(edx&(1U<<9)))return-1;
    uint64_t base=read_msr(APIC_MSR_BASE);base|=1ULL<<11;base&=~(1ULL<<10);write_msr(APIC_MSR_BASE,base);
    uint64_t lapic_physical=platform->local_apic_address?platform->local_apic_address:base&0xfffff000ULL;
    lapic=paging_map_mmio(lapic_physical,4096);if(!lapic)return-1;
    bsp_apic_id=(uint8_t)(lapic_read(LAPIC_ID)>>24);ioapic_count=0;
    for(uint8_t i=0;i<platform->ioapic_count;i++){
        volatile uint32_t*registers=paging_map_mmio(platform->ioapics[i].address,4096);if(!registers)continue;
        ioapic_runtime_t*io=&ioapics[ioapic_count];io->registers=registers;io->gsi_base=platform->ioapics[i].gsi_base;io->redirection_count=((ioapic_read(io,1)>>16)&0xff)+1;
        if(!io->redirection_count||io->redirection_count>120)continue;
        for(uint32_t entry=0;entry<io->redirection_count;entry++){uint8_t reg=(uint8_t)(0x10+entry*2);ioapic_write(io,reg,1U<<16);ioapic_write(io,(uint8_t)(reg+1),(uint32_t)bsp_apic_id<<24);}
        ioapic_count++;
    }
    if(!ioapic_count){lapic=0;return-1;}
    lapic_write(LAPIC_TPR,0);lapic_write(LAPIC_LVT_TIMER,LAPIC_MASKED);lapic_write(LAPIC_LVT_THERMAL,LAPIC_MASKED);lapic_write(LAPIC_LVT_PERFORMANCE,LAPIC_MASKED);
    lapic_write(LAPIC_LVT_LINT0,LAPIC_MASKED);lapic_write(LAPIC_LVT_LINT1,LAPIC_MASKED);lapic_write(LAPIC_LVT_ERROR,LAPIC_MASKED);lapic_write(LAPIC_SVR,(1U<<8)|255U);lapic_write(LAPIC_EOI,0);
    outb(0x21,0xff);outb(0xa1,0xff);active=1;serial_write("APIC: local APIC and IOAPIC routing ready\n");return 0;
}

int apic_is_active(void){return active!=0;}

int apic_route_legacy_irq(uint8_t irq,uint8_t vector){
    if(!active||irq>=16||vector<32)return-1;const acpi_platform_info_t*platform=acpi_platform_info();uint32_t gsi=irq;uint16_t flags=0;
    for(uint8_t i=0;i<platform->interrupt_override_count;i++)if(platform->interrupt_overrides[i].source_irq==irq){gsi=platform->interrupt_overrides[i].gsi;flags=platform->interrupt_overrides[i].flags;break;}
    ioapic_runtime_t*io=ioapic_for_gsi(gsi);if(!io)return-1;uint32_t entry=gsi-io->gsi_base;uint8_t reg=(uint8_t)(0x10+entry*2);uint32_t low=vector;
    if((flags&3)==3)low|=1U<<13;if(((flags>>2)&3)==3)low|=1U<<15;
    ioapic_write(io,(uint8_t)(reg+1),(uint32_t)bsp_apic_id<<24);ioapic_write(io,reg,low);
    uint32_t actual=ioapic_read(io,reg);return(actual&0x100ffU)==vector?0:-1;
}

void apic_send_eoi(void){if(lapic)lapic_write(LAPIC_EOI,0);}
