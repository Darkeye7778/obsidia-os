#include "mouse.h"
#include "../idt.h"
#include "../resource.h"
#include <stdint.h>

#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_DATA 0x60

extern void serial_write(const char*text);

typedef struct {int16_t dx,dy;uint8_t buttons,x_overflow,y_overflow;} decoded_packet_t;

static uint8_t packet[3],packet_index,old_buttons;
static uint64_t irq_count,packet_count,motion_event_count,overflow_count,resync_count,absolute_dx,absolute_dy;

static void serial_u64(uint64_t value){char text[21];int i=20;text[i]=0;if(!value){serial_write("0");return;}while(value&&i){text[--i]=(char)('0'+value%10);value/=10;}serial_write(&text[i]);}
static int wait_write(void){for(uint32_t i=0;i<100000;i++)if(!(inb(PS2_STATUS)&2))return 1;return 0;}
static int wait_read(void){for(uint32_t i=0;i<100000;i++)if(inb(PS2_STATUS)&1)return 1;return 0;}
static int controller_command(uint8_t command){if(!wait_write())return 0;outb(PS2_COMMAND,command);return 1;}
static int mouse_write(uint8_t value){if(!controller_command(0xd4)||!wait_write())return 0;outb(PS2_DATA,value);return 1;}
static int mouse_ack(void){return wait_read()&&inb(PS2_DATA)==0xfa;}
static int mouse_command(uint8_t command){return mouse_write(command)&&mouse_ack();}
static int mouse_status(uint8_t*status,uint8_t*resolution,uint8_t*sample_rate){
    if(!mouse_command(0xe9)||!wait_read())return 0;*status=inb(PS2_DATA);
    if(!wait_read())return 0;*resolution=inb(PS2_DATA);
    if(!wait_read())return 0;*sample_rate=inb(PS2_DATA);return 1;
}
static void print_device_status(const char*stage,uint8_t status,uint8_t resolution,uint8_t sample_rate){
    serial_write("MOUSE: ");serial_write(stage);serial_write(" scaling=");serial_write(status&0x10?"2:1":"1:1");serial_write(" resolution=");
    if(resolution<=3)serial_u64(1U<<resolution);else serial_u64(resolution);serial_write(" counts/mm sample=");serial_u64(sample_rate);serial_write(" Hz\n");
}

static int16_t signed_axis(uint8_t value,uint8_t sign){return sign?(int16_t)value-256:(int16_t)value;}
static int decode_packet(const uint8_t bytes[3],decoded_packet_t*out){
    if(!(bytes[0]&0x08)||!out)return 0;
    out->buttons=bytes[0]&7;out->x_overflow=(bytes[0]>>6)&1;out->y_overflow=(bytes[0]>>7)&1;
    out->dx=out->x_overflow?0:signed_axis(bytes[1],bytes[0]&0x10);
    /* Hardware Y is positive upward; convert once to screen-positive downward. */
    int16_t hardware_dy=signed_axis(bytes[2],bytes[0]&0x20);out->dy=out->y_overflow?0:(int16_t)-hardware_dy;
    return 1;
}
static int parser_feed(uint8_t byte,decoded_packet_t*out){
    if(!packet_index&&!(byte&0x08)){resync_count++;return 0;}
    packet[packet_index++]=byte;if(packet_index<3)return 0;packet_index=0;packet_count++;
    if(!decode_packet(packet,out)){resync_count++;return 0;}overflow_count+=(uint64_t)out->x_overflow+out->y_overflow;return 1;
}
static int parser_self_test(void){
    decoded_packet_t out;packet_index=0;resync_count=packet_count=overflow_count=0;
    if(parser_feed(0,&out)||resync_count!=1)return 0;
    if(parser_feed(0x18,&out)||parser_feed(0xff,&out)||!parser_feed(0,&out)||out.dx!=-1||out.dy!=0)return 0;
    if(parser_feed(0x28,&out)||parser_feed(0,&out)||!parser_feed(0xff,&out)||out.dx!=0||out.dy!=1)return 0;
    if(parser_feed(0xc8,&out)||parser_feed(0x7f,&out)||!parser_feed(0x7f,&out)||out.dx||out.dy||!out.x_overflow||!out.y_overflow)return 0;
    packet_index=0;resync_count=packet_count=overflow_count=0;return 1;
}

static void push_button(uint8_t bit,uint32_t code,uint8_t buttons,int pressed){
    if(((buttons^old_buttons)&bit)&&!!(buttons&bit)==pressed)resource_input_push(INPUT_EVENT_BUTTON,code,pressed);
}
static void emit_packet(const decoded_packet_t*p){
    push_button(1,INPUT_BUTTON_LEFT,p->buttons,1);push_button(2,INPUT_BUTTON_RIGHT,p->buttons,1);push_button(4,INPUT_BUTTON_MIDDLE,p->buttons,1);
    if(p->dx||p->dy){resource_input_push_motion(p->dx,p->dy);motion_event_count++;absolute_dx+=(uint64_t)(p->dx<0?-p->dx:p->dx);absolute_dy+=(uint64_t)(p->dy<0?-p->dy:p->dy);}
    push_button(1,INPUT_BUTTON_LEFT,p->buttons,0);push_button(2,INPUT_BUTTON_RIGHT,p->buttons,0);push_button(4,INPUT_BUTTON_MIDDLE,p->buttons,0);old_buttons=p->buttons;
}
static void print_summary(void){
    serial_write("MOUSE: irq=");serial_u64(irq_count);serial_write(" packets=");serial_u64(packet_count);serial_write(" motion=");serial_u64(motion_event_count);
    serial_write(" abs_dx=");serial_u64(absolute_dx);serial_write(" abs_dy=");serial_u64(absolute_dy);serial_write(" overflow=");serial_u64(overflow_count);serial_write(" resync=");serial_u64(resync_count);
    serial_write(" queue_drop=");serial_u64(resource_input_dropped_motion());serial_write(" coalesced=");serial_u64(resource_input_coalesced_motion());serial_write("\n");
}
static void mouse_irq_handler(registers_t*registers){
    (void)registers;uint8_t status=inb(PS2_STATUS);
    /* IRQ12 only consumes auxiliary bytes; keyboard data remains on IRQ1. */
    if(!(status&1)||!(status&0x20))return;
    uint8_t byte=inb(PS2_DATA);irq_count++;decoded_packet_t decoded;if(!parser_feed(byte,&decoded))return;emit_packet(&decoded);if(!(packet_count&255U))print_summary();
}

void mouse_init(void){
    packet_index=old_buttons=0;irq_count=packet_count=motion_event_count=overflow_count=resync_count=absolute_dx=absolute_dy=0;
    if(!parser_self_test()){serial_write("PS/2 mouse parser self-test FAILED\n");return;}serial_write("PS/2 mouse parser self-test passed\n");
    if(!controller_command(0xa8)||!controller_command(0x20)||!wait_read()){serial_write("PS/2 mouse unavailable\n");return;}
    uint8_t configuration=inb(PS2_DATA);configuration|=2;configuration&=(uint8_t)~0x20;
    if(!controller_command(0x60)||!wait_write()){serial_write("PS/2 mouse configuration failed\n");return;}outb(PS2_DATA,configuration);
    /* Measure the device baseline, then establish a known linear mode. */
    uint8_t device_status,device_resolution,device_sample_rate;
    if(!mouse_command(0xf6)||!mouse_status(&device_status,&device_resolution,&device_sample_rate)){
        serial_write("PS/2 mouse defaults/status failed\n");return;
    }
    print_device_status("defaults",device_status,device_resolution,device_sample_rate);
    if(!mouse_command(0xe6)||!mouse_command(0xe8)||!mouse_command(3)||!mouse_command(0xf3)||!mouse_command(200)||!mouse_status(&device_status,&device_resolution,&device_sample_rate)){
        serial_write("PS/2 mouse parameter configuration failed\n");return;
    }
    print_device_status("configured",device_status,device_resolution,device_sample_rate);
    idt_set_handler(44,mouse_irq_handler);if(!mouse_command(0xf4)){serial_write("PS/2 mouse enable failed\n");return;}
    interrupt_unmask_irq(12);
    serial_write("PS/2 mouse initialized: stream, scaling 1:1, resolution 8 counts/mm, sample 200 Hz\n");
}
