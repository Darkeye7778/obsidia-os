#include "pci.h"
#include "console/console.h"
#include "idt.h"  // for outb/inb if not global, but we have them via block or idt
#include <stdint.h>

// Use inb/outb from idt (they are global)
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);

void pci_init(void) {
    console_print("PCI skeleton initialized\n");
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    outb(0xCF8, (address >> 0) & 0xFF);  // note: full 32 bit write needed but simplified for hobby
    // Better full write:
    // Actually for simplicity use full 32bit out to 0xCF8
    // Use asm for 32bit
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(0xCF8));
    uint32_t tmp;
    __asm__ volatile ("inl %1, %0" : "=a"(tmp) : "Nd"(0xCFC));
    return tmp;
}

void pci_config_write32(uint8_t bus,uint8_t slot,uint8_t func,uint8_t offset,uint32_t value){
    uint32_t address=0x80000000U|((uint32_t)bus<<16)|((uint32_t)slot<<11)|((uint32_t)func<<8)|(offset&0xfc);
    __asm__ volatile("outl %0,%1"::"a"(address),"Nd"(0xcf8));__asm__ volatile("outl %0,%1"::"a"(value),"Nd"(0xcfc));
}

int pci_find_class(uint8_t wanted_class,uint8_t wanted_subclass,uint8_t wanted_interface,pci_device_t*result){
    if(!result)return-1;
    for(uint16_t bus=0;bus<256;bus++)for(uint8_t slot=0;slot<32;slot++){
        uint32_t first=pci_config_read32((uint8_t)bus,slot,0,0);if((first&0xffff)==0xffff)continue;
        uint8_t header=(uint8_t)(pci_config_read32((uint8_t)bus,slot,0,0x0c)>>16);uint8_t functions=(header&0x80)?8:1;
        for(uint8_t function=0;function<functions;function++){
            uint32_t id=pci_config_read32((uint8_t)bus,slot,function,0);if((id&0xffff)==0xffff)continue;uint32_t type=pci_config_read32((uint8_t)bus,slot,function,8);
            uint8_t class_code=(uint8_t)(type>>24),subclass=(uint8_t)(type>>16),interface=(uint8_t)(type>>8);
            if(class_code==wanted_class&&subclass==wanted_subclass&&interface==wanted_interface){*result=(pci_device_t){(uint8_t)bus,slot,function,(uint16_t)id,(uint16_t)(id>>16),class_code,subclass,interface,(uint8_t)(pci_config_read32((uint8_t)bus,slot,function,0x0c)>>16)};return 0;}
        }
    }
    return-1;
}
int pci_enable_memory_bus_master(const pci_device_t*device){if(!device)return-1;uint32_t command=pci_config_read32(device->bus,device->slot,device->function,4);command|=6;pci_config_write32(device->bus,device->slot,device->function,4,command);return(pci_config_read32(device->bus,device->slot,device->function,4)&6)==6?0:-1;}
int pci_bar_memory_address(const pci_device_t*device,uint8_t bar,uint64_t*address){
    if(!device||!address||bar>=6)return-1;uint8_t offset=(uint8_t)(0x10+bar*4);uint32_t low=pci_config_read32(device->bus,device->slot,device->function,offset);if((low&1)||!(low&~0xfU))return-1;
    uint32_t kind=(low>>1)&3;uint64_t value=low&~0xfU;if(kind==2){if(bar==5)return-1;value|=(uint64_t)pci_config_read32(device->bus,device->slot,device->function,offset+4)<<32;}else if(kind!=0)return-1;*address=value;return 0;
}
int pci_legacy_interrupt(const pci_device_t*device,uint8_t*irq_line,uint8_t*interrupt_pin){
    if(!device||!irq_line||!interrupt_pin)return-1;uint32_t value=pci_config_read32(device->bus,device->slot,device->function,0x3c);uint8_t line=(uint8_t)value,pin=(uint8_t)(value>>8);
    if(!pin||pin>4||line>=16)return-1;*irq_line=line;*interrupt_pin=pin;return 0;
}

void pci_get_class(uint8_t bus, uint8_t slot, uint8_t func, uint8_t* class_code, uint8_t* subclass) {
    uint32_t reg = pci_config_read32(bus, slot, func, 0x08);
    *class_code = (reg >> 24) & 0xFF;
    *subclass = (reg >> 16) & 0xFF;
}

void pci_scan(void) {
    console_print("PCI scan (skeleton):\n");
    for (uint16_t bus = 0; bus < 1; bus++) {  // limit to bus 0 for speed/safety
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_config_read32(bus, slot, func, 0x00);
                if ((id & 0xFFFF) != 0xFFFF) {  // valid device
                    uint8_t classc, sub;
                    pci_get_class(bus, slot, func, &classc, &sub);
                    console_print("  found dev at ");
                    // simple hex print not full, but note
                    console_print("bus/slot/func (class/sub) ");
                    // for demo just count or print basic
                    // To keep simple, print when mass storage or network found
                    if (classc == 0x01) { // mass storage
                        console_print("MASS STORAGE ");
                    } else if (classc == 0x02) {
                        console_print("NET ");
                    }
                    console_print("\n");
                }
            }
        }
    }
    console_print("PCI scan complete (limited).\n");
}
