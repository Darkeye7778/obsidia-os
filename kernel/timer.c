#include "timer.h"
#include "idt.h"
#include "console/console.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193182

static volatile uint64_t timer_ticks = 0;

static void timer_irq_handler(registers_t* regs) {
    (void)regs;
    timer_ticks++;
}

void timer_init(void) {
    // ~100 Hz
    uint32_t divisor = PIT_FREQ / 100;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    outb(PIT_COMMAND, 0x36); // channel 0, lobyte/hibyte, square wave
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    idt_set_handler(32, timer_irq_handler);

    // Already unmasked in keyboard_init (bit 0), but ensure
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 0);
    outb(0x21, mask);

    console_print("Timer (PIT) initialized (~100Hz)\n");
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_sleep(uint64_t ticks) {
    uint64_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        __asm__ volatile ("hlt");
    }
}
