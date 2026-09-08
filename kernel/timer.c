#include "timer.h"
#include "idt.h"
#include "console/console.h"
#include "hpet.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQ     1193182

static volatile uint64_t timer_ticks = 0;

static void timer_irq_handler(registers_t* regs) {
    (void)regs;
    timer_ticks++;
    /* Frequent observation makes the extension of a 32-bit HPET counter safe
       across wrap even when no userspace clock client is active. */
    if(hpet_is_available())(void)hpet_monotonic_ns();
}

void timer_init(void) {
    // ~100 Hz
    uint32_t divisor = PIT_FREQ / 100;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    outb(PIT_COMMAND, 0x36); // channel 0, lobyte/hibyte, square wave
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    idt_add_handler(32, timer_irq_handler);

    // Already unmasked in keyboard_init (bit 0), but ensure
    interrupt_unmask_irq(0);

    console_print("Timer (PIT) initialized (~100Hz)\n");
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

uint64_t timer_monotonic_ns(void){
    if(hpet_is_available())return hpet_monotonic_ns();
    return timer_ticks*10000000ULL;
}
uint64_t timer_monotonic_resolution_ns(void){
    uint64_t resolution=hpet_resolution_ns();
    return resolution?resolution:10000000ULL;
}

void timer_sleep(uint64_t ticks) {
    uint64_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        __asm__ volatile ("hlt");
    }
}
