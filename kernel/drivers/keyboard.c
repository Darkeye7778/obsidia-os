#include "keyboard.h"
#include "idt.h"
#include <stdint.h>
#include "../task.h"

static int shift_down = 0;
static int ctrl_down = 0;
static int caps_lock = 0;
static int extended = 0;

static const char normal_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0',

    [0x0C] = '-', [0x0D] = '=',

    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p',

    [0x1A] = '[', [0x1B] = ']',

    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l',

    [0x27] = ';', [0x28] = '\'', [0x29] = '`',

    [0x2B] = '\\',

    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',

    [0x33] = ',', [0x34] = '.', [0x35] = '/',

    [0x39] = ' ',
    [0x1C] = '\n',
    [0x0E] = '\b',
};

static const char shift_map[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')',

    [0x0C] = '_', [0x0D] = '+',

    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P',

    [0x1A] = '{', [0x1B] = '}',

    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L',

    [0x27] = ':', [0x28] = '"', [0x29] = '~',

    [0x2B] = '|',

    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',

    [0x33] = '<', [0x34] = '>', [0x35] = '?',

    [0x39] = ' ',
    [0x1C] = '\n',
    [0x0E] = '\b',
};

static int is_letter(char c) {
    return c >= 'a' && c <= 'z';
}

static char uppercase(char c) {
    if (is_letter(c)) {
        return c - 'a' + 'A';
    }
    return c;
}

// ===== IRQ driven input (new) =====
#define KEYBUF_SIZE 64
static int keybuf[KEYBUF_SIZE];
static int keybuf_head = 0;
static int keybuf_tail = 0;

static void enqueue_key(int k) {
    int next = (keybuf_head + 1) % KEYBUF_SIZE;
    if (next == keybuf_tail) return; // drop on full
    keybuf[keybuf_head] = k;
    keybuf_head = next;
}

static int dequeue_key(void) {
    if (keybuf_head == keybuf_tail) return 0;
    int k = keybuf[keybuf_tail];
    keybuf_tail = (keybuf_tail + 1) % KEYBUF_SIZE;
    return k;
}

// Keyboard IRQ handler (called for vector 33 / IRQ1)
static void keyboard_irq_handler(registers_t* regs) {
    (void)regs;

    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0) {
        extended = 1;
        return;
    }

    int released = scancode & 0x80;
    uint8_t code = scancode & 0x7F;

    if (extended) {
        extended = 0;
        if (released) return;
        int special = 0;
        if (code == 0x4B) special = KEY_ARROW_LEFT;
        else if (code == 0x4D) special = KEY_ARROW_RIGHT;
        else if (code == 0x48) special = KEY_ARROW_UP;
        else if (code == 0x50) special = KEY_ARROW_DOWN;
        if (special) { enqueue_key(special); task_wake_input_waiters(); }
        return;
    }

    if (code == 0x2A || code == 0x36) {
        shift_down = !released;
        return;
    }

    if (code == 0x1D) {
        ctrl_down = !released;
        return;
    }

    if (code == 0x3A && !released) {
        caps_lock = !caps_lock;
        return;
    }

    if (released) {
        return;
    }

    char c = 0;
    if (shift_down) {
        c = shift_map[code];
    } else {
        c = normal_map[code];
        if (caps_lock && is_letter(c)) c = uppercase(c);
    }

    if (!c) {
        return;
    }

    int key = (int)c;
    if (ctrl_down) {
        if (c == 'a' || c == 'A') key = KEY_CTRL_A;
        else if (c == 'c' || c == 'C') key = KEY_CTRL_C;
        else if (c == 'l' || c == 'L') key = KEY_CTRL_L;
        else return;
    }

    enqueue_key(key);
    task_wake_input_waiters();
}

int keyboard_try_getkey(void) { return dequeue_key(); }

void keyboard_init(void) {
    keybuf_head = keybuf_tail = 0;

    // Register our C handler for IRQ1 (vector 33 after PIC remap)
    idt_set_handler(33, keyboard_irq_handler);

    // Unmask IRQ0 (timer bit0) and IRQ1 (keyboard bit1) on master PIC
    uint8_t mask = inb(0x21);
    mask &= ~((1<<0) | (1<<1));
    outb(0x21, mask);
}

int keyboard_getkey(void) {
    // Wait for a key (works both polled early and IRQ later)
    while (keybuf_head == keybuf_tail) {
        __asm__ volatile ("hlt");
    }
    return dequeue_key();
}
