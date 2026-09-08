#include "acpi.h"
#include "idt.h"
#include <stdint.h>

#define ACPI_TABLE_LIMIT (1024U*1024U)

static acpi_platform_info_t platform;
static struct limine_memmap_response* map;
static uint64_t direct_offset;
static uint16_t smi_command;
static uint8_t acpi_enable_value;
static uint8_t reset_space,reset_width,reset_value;
static uint64_t reset_address;

extern void serial_write(const char*);

static uint16_t read16(const uint8_t*p){return(uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t read32(const uint8_t*p){return(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t read64(const uint8_t*p){uint64_t value=0;for(uint32_t i=0;i<8;i++)value|=(uint64_t)p[i]<<(i*8);return value;}
static int bytes_equal(const uint8_t*a,const char*b,uint32_t length){for(uint32_t i=0;i<length;i++)if(a[i]!=(uint8_t)b[i])return 0;return 1;}
static int checksum(const uint8_t*bytes,uint32_t length){uint8_t sum=0;for(uint32_t i=0;i<length;i++)sum=(uint8_t)(sum+bytes[i]);return sum==0;}

static int physical_range_valid(uint64_t address,uint64_t length){
    if(!map||!length||address>UINT64_MAX-length)return 0;
    for(uint64_t i=0;i<map->entry_count;i++){
        struct limine_memmap_entry*entry=map->entries[i];
        if(entry&&entry->base<=UINT64_MAX-entry->length&&address>=entry->base&&address+length<=entry->base+entry->length&&
           entry->type!=LIMINE_MEMMAP_BAD_MEMORY)return 1;
    }
    return 0;
}
static uint8_t*physical_pointer(uint64_t address,uint64_t length){return physical_range_valid(address,length)&&address<=UINT64_MAX-direct_offset?(uint8_t*)(address+direct_offset):0;}
static uint8_t*valid_table(uint64_t address){
    uint8_t*header=physical_pointer(address,36);if(!header)return 0;uint32_t length=read32(header+4);
    if(length<36||length>ACPI_TABLE_LIMIT||!physical_range_valid(address,length))return 0;
    if(address>UINT64_MAX-direct_offset)return 0;header=(uint8_t*)(address+direct_offset);return checksum(header,length)?header:0;
}

static int aml_integer(const uint8_t**cursor,const uint8_t*end,uint64_t*value){
    if(*cursor>=end)return-1;uint8_t op=*(*cursor)++;
    if(op==0x00){*value=0;return 0;}if(op==0x01){*value=1;return 0;}
    uint32_t bytes=op==0x0a?1:op==0x0b?2:op==0x0c?4:op==0x0e?8:0;if(!bytes||end-*cursor<(int64_t)bytes)return-1;
    *value=0;for(uint32_t i=0;i<bytes;i++)*value|=(uint64_t)(*cursor)[i]<<(i*8);*cursor+=bytes;return 0;
}
static int parse_s5(uint64_t dsdt_address){
    uint8_t*dsdt=valid_table(dsdt_address);if(!dsdt||!bytes_equal(dsdt,"DSDT",4))return-1;uint32_t length=read32(dsdt+4);
    for(uint32_t i=36;i+6<length;i++)if(bytes_equal(dsdt+i,"_S5_",4)){
        int named=(i&&dsdt[i-1]==0x08)||(i>1&&dsdt[i-2]==0x08&&dsdt[i-1]=='\\');if(!named||dsdt[i+4]!=0x12)continue;
        const uint8_t*cursor=dsdt+i+5,*end=dsdt+length;uint8_t lead=*cursor++;uint32_t follow=lead>>6;uint32_t package_length=follow?lead&0x0f:lead&0x3f;
        if(follow>3||end-cursor<(int64_t)follow)return-1;for(uint32_t n=0;n<follow;n++)package_length|=(uint32_t)*cursor++<<(4+n*8);
        if(!package_length||cursor>=end)continue;const uint8_t*package_end=cursor+package_length-1;if(package_end>end)continue;
        uint8_t elements=*cursor++;uint64_t a=0,b=0;if(elements<2||aml_integer(&cursor,package_end,&a)||aml_integer(&cursor,package_end,&b)||a>7||b>7)continue;
        platform.sleep_type_a=(uint8_t)a;platform.sleep_type_b=(uint8_t)b;platform.has_s5=1;return 0;
    }
    return-1;
}

static void parse_fadt(const uint8_t*fadt){
    uint32_t length=read32(fadt+4);if(length<90)return;platform.has_fadt=1;
    uint64_t dsdt=read32(fadt+40);smi_command=(uint16_t)read32(fadt+48);acpi_enable_value=fadt[52];
    uint32_t pm1a=read32(fadt+64),pm1b=read32(fadt+68);
    if(length>=148&&read64(fadt+140))dsdt=read64(fadt+140);
    if(!pm1a&&length>=184&&fadt[172]==1)pm1a=(uint32_t)read64(fadt+176);
    if(!pm1b&&length>=196&&fadt[184]==1)pm1b=(uint32_t)read64(fadt+188);
    if(pm1a<=0xffff)platform.pm1a_control=(uint16_t)pm1a;if(pm1b<=0xffff)platform.pm1b_control=(uint16_t)pm1b;
    if(length>=129){reset_space=fadt[116];reset_width=fadt[117];reset_address=read64(fadt+120);reset_value=fadt[128];if(reset_space==1&&reset_address<=0xffff)platform.has_reset=1;}
    if(dsdt)parse_s5(dsdt);
}

static void parse_madt(const uint8_t*madt){
    uint32_t length=read32(madt+4);if(length<44)return;
    platform.local_apic_address=read32(madt+36);
    for(uint32_t offset=44;offset+2<=length;){
        const uint8_t*entry=madt+offset;uint8_t type=entry[0],entry_length=entry[1];
        if(entry_length<2||entry_length>length-offset)return;
        if(type==1&&entry_length>=12&&platform.ioapic_count<ACPI_MAX_IOAPICS){
            acpi_ioapic_t*io=&platform.ioapics[platform.ioapic_count++];
            io->id=entry[2];io->address=read32(entry+4);io->gsi_base=read32(entry+8);
        }else if(type==2&&entry_length>=10&&entry[2]==0&&platform.interrupt_override_count<ACPI_MAX_INTERRUPT_OVERRIDES){
            acpi_interrupt_override_t*override=&platform.interrupt_overrides[platform.interrupt_override_count++];
            override->source_irq=entry[3];override->gsi=read32(entry+4);override->flags=read16(entry+8);
        }else if(type==5&&entry_length>=12){
            platform.local_apic_address=read64(entry+4);
        }
        offset+=entry_length;
    }
    if(platform.local_apic_address&&platform.ioapic_count)platform.has_madt=1;
}

int acpi_init(void*rsdp_address,uint64_t hhdm_offset,struct limine_memmap_response*memory_map){
    for(uint32_t i=0;i<sizeof(platform);i++)((uint8_t*)&platform)[i]=0;map=memory_map;direct_offset=hhdm_offset;
    uint8_t*rsdp=rsdp_address;if(!rsdp||!bytes_equal(rsdp,"RSD PTR ",8)||!checksum(rsdp,20))return-1;platform.revision=rsdp[15];
    uint64_t root=read32(rsdp+16);int xsdt=0;if(platform.revision>=2){uint32_t length=read32(rsdp+20);if(length<36||length>4096||!checksum(rsdp,length))return-1;if(read64(rsdp+24)){root=read64(rsdp+24);xsdt=1;}}
    uint8_t*table=valid_table(root);if(!table||!bytes_equal(table,xsdt?"XSDT":"RSDT",4))return-1;uint32_t length=read32(table+4),entry_size=xsdt?8:4,count=(length-36)/entry_size;
    if(count>256)return-1;for(uint32_t i=0;i<count;i++){uint64_t address=xsdt?read64(table+36+i*8):read32(table+36+i*4);uint8_t*child=valid_table(address);if(!child)continue;if(bytes_equal(child,"FACP",4))parse_fadt(child);else if(bytes_equal(child,"APIC",4))parse_madt(child);}
    if(!platform.has_fadt)return-1;serial_write(platform.has_s5?"ACPI: tables and S5 power state ready\n":"ACPI: tables ready; S5 unavailable\n");return 0;
}

const acpi_platform_info_t*acpi_platform_info(void){return&platform;}
static uint16_t inw(uint16_t port){uint16_t value;__asm__ volatile("inw %1,%0":"=a"(value):"Nd"(port));return value;}
static void outw(uint16_t port,uint16_t value){__asm__ volatile("outw %0,%1"::"a"(value),"Nd"(port));}
static void outl(uint16_t port,uint32_t value){__asm__ volatile("outl %0,%1"::"a"(value),"Nd"(port));}

int acpi_power_off(void){
    if(!platform.has_s5||!platform.pm1a_control)return-1;
    if(!(inw(platform.pm1a_control)&1)&&smi_command&&acpi_enable_value){outb(smi_command,acpi_enable_value);for(uint32_t wait=0;wait<1000000&&!(inw(platform.pm1a_control)&1);wait++)__asm__ volatile("pause");}
    outw(platform.pm1a_control,(uint16_t)((platform.sleep_type_a<<10)|(1<<13)));if(platform.pm1b_control)outw(platform.pm1b_control,(uint16_t)((platform.sleep_type_b<<10)|(1<<13)));return 0;
}
int acpi_reboot(void){
    if(platform.has_reset){if(reset_width<=8)outb((uint16_t)reset_address,reset_value);else if(reset_width<=16)outw((uint16_t)reset_address,reset_value);else outl((uint16_t)reset_address,reset_value);}
    /* Standard i8042 CPU reset is the architecture fallback when firmware did
       not provide, or failed to honor, its reset register. */
    for(uint32_t wait=0;wait<100000&&inb(0x64)&2;wait++)__asm__ volatile("pause");outb(0x64,0xfe);return 0;
}
