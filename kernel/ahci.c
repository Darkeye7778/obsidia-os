#include "ahci.h"
#include "block.h"
#include "pci.h"
#include "paging.h"
#include "dma.h"
#include "idt.h"
#include "apic.h"
#include "timer.h"
#include "memory/heap.h"
#include <stdint.h>

#define AHCI_MAX_PORTS 32U
#define AHCI_MAX_DEVICES 4U
#define AHCI_BOUNCE_BYTES (64U*1024U)
#define AHCI_TIMEOUT 2000000U
#define AHCI_PORT_BASE 0x100U
#define AHCI_PORT_STRIDE 0x80U
#define AHCI_PORT_SATA_SIGNATURE 0x00000101U

enum { PORT_CLB=0x00,PORT_CLBU=0x04,PORT_FB=0x08,PORT_FBU=0x0c,PORT_IS=0x10,PORT_IE=0x14,PORT_CMD=0x18,PORT_TFD=0x20,PORT_SIG=0x24,PORT_SSTS=0x28,PORT_SERR=0x30,PORT_SACT=0x34,PORT_CI=0x38 };

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t prdt_length;
    uint32_t transferred;
    uint32_t table_base;
    uint32_t table_base_upper;
    uint32_t reserved[4];
} command_header_t;
typedef struct __attribute__((packed)) {uint32_t base,base_upper,reserved,byte_count;} prdt_entry_t;
typedef struct __attribute__((packed)) {uint8_t cfis[64],atapi[16],reserved[48];prdt_entry_t prdt[1];} command_table_t;

typedef struct {
    volatile uint32_t*registers;
    dma_buffer_t command_list,received_fis,command_table,bounce;
    uint64_t sectors;
    uint8_t port_number;
    block_device_t* device;
    volatile uint32_t last_interrupt;
    uint64_t interrupt_count;
} ahci_disk_t;

extern void serial_write(const char*);
static volatile uint32_t*controller_abar;
static ahci_disk_t*controller_disks[AHCI_MAX_PORTS];
static uint64_t controller_irq_count;
static uint8_t controller_interrupts,controller_interrupt_kind;
static uint32_t read_reg(ahci_disk_t*disk,uint32_t offset){return disk->registers[offset/4];}
static void write_reg(ahci_disk_t*disk,uint32_t offset,uint32_t value){disk->registers[offset/4]=value;__asm__ volatile("mfence":::"memory");}
static int wait_clear(ahci_disk_t*disk,uint32_t offset,uint32_t mask){for(uint32_t n=0;n<AHCI_TIMEOUT;n++)if(!(read_reg(disk,offset)&mask))return 0;return-1;}
static void wait_for_interrupt(void){
    uint64_t flags;__asm__ volatile("pushfq; pop %0":"=r"(flags));
    if(flags&(1ULL<<9))__asm__ volatile("hlt");
    else __asm__ volatile("sti; hlt; cli":::"memory");
}

static void release_disk(ahci_disk_t*disk){if(!disk)return;dma_release(&disk->bounce);dma_release(&disk->command_table);dma_release(&disk->received_fis);dma_release(&disk->command_list);kfree(disk);}
static int allocate_disk(ahci_disk_t*disk){
    return dma_allocate(1024,0xffffffffULL,&disk->command_list)||dma_allocate(256,0xffffffffULL,&disk->received_fis)||
           dma_allocate(256,0xffffffffULL,&disk->command_table)||dma_allocate(AHCI_BOUNCE_BYTES,0xffffffffULL,&disk->bounce)?-1:0;
}
static int stop_port(ahci_disk_t*disk){uint32_t command=read_reg(disk,PORT_CMD);command&=~1U;write_reg(disk,PORT_CMD,command);if(wait_clear(disk,PORT_CMD,1U<<15))return-1;command&=~(1U<<4);write_reg(disk,PORT_CMD,command);return wait_clear(disk,PORT_CMD,1U<<14);}
static int start_port(ahci_disk_t*disk){
    if(stop_port(disk))return-1;write_reg(disk,PORT_CLB,(uint32_t)disk->command_list.physical_address);write_reg(disk,PORT_CLBU,(uint32_t)(disk->command_list.physical_address>>32));write_reg(disk,PORT_FB,(uint32_t)disk->received_fis.physical_address);write_reg(disk,PORT_FBU,(uint32_t)(disk->received_fis.physical_address>>32));
    command_header_t*header=disk->command_list.virtual_address;header[0].table_base=(uint32_t)disk->command_table.physical_address;header[0].table_base_upper=(uint32_t)(disk->command_table.physical_address>>32);
    write_reg(disk,PORT_SERR,0xffffffffU);write_reg(disk,PORT_IS,0xffffffffU);write_reg(disk,PORT_IE,controller_interrupts?((1U<<0)|(1U<<5)|(1U<<30)):0);
    uint32_t command=read_reg(disk,PORT_CMD)|(1U<<4)|(1U<<1)|(1U<<2);write_reg(disk,PORT_CMD,command);write_reg(disk,PORT_CMD,command|1U);return 0;
}

static int issue_once(ahci_disk_t*disk,uint8_t command,uint64_t lba,uint16_t sectors,void*buffer,int write){
    uint32_t bytes=(uint32_t)sectors*512U;if(bytes>AHCI_BOUNCE_BYTES||(!buffer&&bytes))return-1;if(write&&bytes)for(uint32_t i=0;i<bytes;i++)((uint8_t*)disk->bounce.virtual_address)[i]=((const uint8_t*)buffer)[i];
    command_header_t*header=disk->command_list.virtual_address;command_table_t*table=disk->command_table.virtual_address;for(uint32_t i=0;i<sizeof(*table);i++)((uint8_t*)table)[i]=0;
    header[0].flags=(uint16_t)(5U|(write?(1U<<6):0));header[0].prdt_length=bytes?1:0;header[0].transferred=0;
    if(bytes){table->prdt[0].base=(uint32_t)disk->bounce.physical_address;table->prdt[0].base_upper=(uint32_t)(disk->bounce.physical_address>>32);table->prdt[0].byte_count=(bytes-1)|(1U<<31);}
    uint8_t*fis=table->cfis;fis[0]=0x27;fis[1]=0x80;fis[2]=command;fis[4]=(uint8_t)lba;fis[5]=(uint8_t)(lba>>8);fis[6]=(uint8_t)(lba>>16);fis[7]=0x40;fis[8]=(uint8_t)(lba>>24);fis[9]=(uint8_t)(lba>>32);fis[10]=(uint8_t)(lba>>40);fis[12]=(uint8_t)sectors;fis[13]=(uint8_t)(sectors>>8);
    if(wait_clear(disk,PORT_TFD,0x88)||read_reg(disk,PORT_CI)&1)return-1;disk->last_interrupt=0;write_reg(disk,PORT_IS,0xffffffffU);__asm__ volatile("mfence":::"memory");write_reg(disk,PORT_CI,1);
    uint64_t deadline=timer_get_ticks()+200;for(uint32_t n=0;n<AHCI_TIMEOUT;n++){uint32_t interrupt=disk->last_interrupt|read_reg(disk,PORT_IS);if((interrupt&(1U<<30))||(read_reg(disk,PORT_TFD)&1))return-1;if(!(read_reg(disk,PORT_CI)&1)){if(!write&&bytes)for(uint32_t i=0;i<bytes;i++)((uint8_t*)buffer)[i]=((uint8_t*)disk->bounce.virtual_address)[i];return 0;}if(controller_interrupts){if(timer_get_ticks()>=deadline)break;wait_for_interrupt();}else __asm__ volatile("pause");}
    if(disk->device)disk->device->timeouts++;return-1;
}
static int recover_port(ahci_disk_t*disk){
    if(stop_port(disk))return-1;write_reg(disk,PORT_SERR,0xffffffffU);uint32_t control=read_reg(disk,0x2c);write_reg(disk,0x2c,(control&~0xfU)|1U);for(uint32_t n=0;n<10000;n++)__asm__ volatile("pause");write_reg(disk,0x2c,control&~0xfU);
    for(uint32_t n=0;n<AHCI_TIMEOUT;n++){uint32_t status=read_reg(disk,PORT_SSTS);if((status&0xf)==3&&((status>>8)&0xf)==1)return start_port(disk);}return-1;
}
static int issue(ahci_disk_t*disk,uint8_t command,uint64_t lba,uint16_t sectors,void*buffer,int write){for(uint32_t attempt=0;attempt<2;attempt++){if(issue_once(disk,command,lba,sectors,buffer,write)==0)return 0;if(attempt==0&&recover_port(disk)==0)continue;break;}if(disk->device)disk->device->online=0;return-1;}
static void identify_text(char*out,uint32_t capacity,const uint16_t*identify,uint32_t first,uint32_t words){uint32_t at=0;for(uint32_t i=0;i<words&&at+1<capacity;i++){uint16_t word=identify[first+i];out[at++]=(char)(word>>8);if(at+1<capacity)out[at++]=(char)word;}while(at&&out[at-1]==' ')at--;out[at]=0;}
static int disk_read(block_device_t*device,uint64_t lba,uint64_t count,void*buffer){ahci_disk_t*disk=device?device->private_data:0;if(!disk||!count||count>128)return-1;return issue(disk,0x25,lba,(uint16_t)count,buffer,0);}
static int disk_write(block_device_t*device,uint64_t lba,uint64_t count,const void*buffer){ahci_disk_t*disk=device?device->private_data:0;if(!disk||!count||count>128)return-1;return issue(disk,0x35,lba,(uint16_t)count,(void*)buffer,1);}
static int disk_flush(block_device_t*device){ahci_disk_t*disk=device?device->private_data:0;return disk?issue(disk,0xea,0,0,0,0):-1;}

static void ahci_interrupt_handler(registers_t*registers){
    (void)registers;if(!controller_abar)return;uint32_t pending=controller_abar[2];if(!pending)return;controller_irq_count++;
    for(uint8_t port=0;port<AHCI_MAX_PORTS;port++)if(pending&(1U<<port)){
        volatile uint32_t*port_registers=controller_abar+(AHCI_PORT_BASE+port*AHCI_PORT_STRIDE)/4;uint32_t status=port_registers[PORT_IS/4];ahci_disk_t*disk=controller_disks[port];
        if(disk){disk->last_interrupt|=status;disk->interrupt_count++;}port_registers[PORT_IS/4]=status;(void)port_registers[PORT_IS/4];
    }
    controller_abar[2]=pending;
}

static int register_port(volatile uint32_t*abar,uint8_t port,uint32_t ordinal){
    ahci_disk_t*disk=kmalloc(sizeof(*disk));if(!disk)return-1;for(uint32_t i=0;i<sizeof(*disk);i++)((uint8_t*)disk)[i]=0;disk->registers=abar+(AHCI_PORT_BASE+port*AHCI_PORT_STRIDE)/4;disk->port_number=port;
    controller_disks[port]=disk;if(allocate_disk(disk)||start_port(disk)||issue(disk,0xec,0,1,disk->bounce.virtual_address,0)){controller_disks[port]=0;release_disk(disk);return-1;}
    uint16_t*identify=disk->bounce.virtual_address;uint64_t sectors=0;if(identify[83]&(1U<<10))sectors=(uint64_t)identify[100]|((uint64_t)identify[101]<<16)|((uint64_t)identify[102]<<32)|((uint64_t)identify[103]<<48);if(!sectors)sectors=(uint64_t)identify[60]|((uint64_t)identify[61]<<16);if(!sectors){release_disk(disk);return-1;}disk->sectors=sectors;
    block_device_t*device=kmalloc(sizeof(*device));if(!device){controller_disks[port]=0;release_disk(disk);return-1;}for(uint32_t i=0;i<sizeof(*device);i++)((uint8_t*)device)[i]=0;const char*prefix="ahci";uint32_t at=0;while(prefix[at]){device->name[at]=prefix[at];at++;}device->name[at++]=(char)('0'+ordinal);device->name[at]=0;
    device->type=BLOCK_TYPE_AHCI;device->block_size=512;device->block_count=sectors;device->read=disk_read;device->write=disk_write;device->flush=disk_flush;device->private_data=disk;device->online=1;disk->device=device;
    char model[41],serial[21];identify_text(model,sizeof(model),identify,27,20);identify_text(serial,sizeof(serial),identify,10,10);block_set_identity(device,model,serial);
    if(block_register(device)){disk->device=0;controller_disks[port]=0;kfree(device);release_disk(disk);return-1;}serial_write("AHCI: disk model: ");serial_write(device->model);serial_write("\n");return 0;
}

int ahci_detect_and_register(void){
    pci_device_t controller;if(pci_find_class(0x01,0x06,0x01,&controller))return 0;uint64_t physical=0;if(pci_bar_memory_address(&controller,5,&physical)||pci_enable_memory_bus_master(&controller))return-1;
    volatile uint32_t*abar=paging_map_mmio(physical,0x2000);if(!abar)return-1;controller_abar=abar;controller_irq_count=0;controller_interrupts=controller_interrupt_kind=0;for(uint8_t i=0;i<AHCI_MAX_PORTS;i++)controller_disks[i]=0;uint32_t cap=abar[0],cap2=abar[9];if(cap2&1){abar[10]|=2;for(uint32_t n=0;n<AHCI_TIMEOUT&&(abar[10]&1);n++)__asm__ volatile("pause");if(abar[10]&1)return-1;}
    abar[1]|=1U<<31;uint8_t msi_vector=0;if(apic_is_active()&&!interrupt_allocate_vector(ahci_interrupt_handler,&msi_vector)){
        if(!pci_enable_msi(&controller,msi_vector,apic_boot_processor_id())){controller_interrupts=1;controller_interrupt_kind=2;}
        else interrupt_release_vector(msi_vector,ahci_interrupt_handler);
    }
    uint8_t irq_line=0,interrupt_pin=0;if(!controller_interrupts&&!pci_legacy_interrupt(&controller,&irq_line,&interrupt_pin)&&!idt_add_handler((uint8_t)(32+irq_line),ahci_interrupt_handler)){if(!interrupt_unmask_pci_irq(irq_line)){controller_interrupts=1;controller_interrupt_kind=1;}}
    if(controller_interrupts)abar[1]|=1U<<1;
    uint32_t implemented=abar[3],registered=0,ports=(cap&31)+1;if(ports>AHCI_MAX_PORTS)ports=AHCI_MAX_PORTS;
    for(uint32_t port=0;port<ports&&registered<AHCI_MAX_DEVICES;port++)if(implemented&(1U<<port)){volatile uint32_t*registers=abar+(AHCI_PORT_BASE+port*AHCI_PORT_STRIDE)/4;uint32_t status=registers[PORT_SSTS/4];if((status&0xf)==3&&((status>>8)&0xf)==1&&registers[PORT_SIG/4]==AHCI_PORT_SATA_SIGNATURE){if(register_port(abar,(uint8_t)port,registered)==0)registered++;}}
    if(registered){serial_write("AHCI: SATA DMA block device registered\n");if(controller_interrupts&&controller_irq_count)serial_write(controller_interrupt_kind==2?"AHCI: MSI completion active\n":"AHCI: PCI INTx completion active\n");else serial_write("AHCI: bounded polling completion active\n");}else serial_write("AHCI: controller present; no usable SATA disk\n");return(int)registered;
}
