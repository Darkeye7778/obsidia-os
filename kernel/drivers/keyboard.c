#include "keyboard.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static int shift_down = 0;
static int ctrl_down = 0;
static int extended = 0;

static const char normal_map[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0',

    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p',

    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l',

    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',

    [0x39] = ' ',
    [0x1C] = '\n',
    [0x0E] = '\b',
};

static const char shift_map[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')',

    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P',

    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L',

    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',

    [0x39] = ' ',
    [0x1C] = '\n',
    [0x0E] = '\b',
};

int keyboard_getkey(void) {
    while (1) {
        if (!(inb(0x64) & 1)) {
            continue;
        }

        uint8_t scancode = inb(0x60);

        if (scancode == 0xE0) {
            extended = 1;
            continue;
        }

        int released = scancode & 0x80;
        uint8_t code = scancode & 0x7F;

        if (extended) {
            extended = 0;

            if (!released) {
                if (code == 0x4B) return KEY_ARROW_LEFT;
                if (code == 0x4D) return KEY_ARROW_RIGHT;
                if (code == 0x48) return KEY_ARROW_UP;
                if (code == 0x50) return KEY_ARROW_DOWN;
            }

            continue;
        }

        if (code == 0x2A || code == 0x36) {
            shift_down = !released;
            continue;
        }

        if (code == 0x1D) {
            ctrl_down = !released;
            continue;
        }

        if (released) {
            continue;
        }

        char c = shift_down ? shift_map[code] : normal_map[code];

        if (ctrl_down && c >= 'a' && c <= 'z') {
            return c - 'a' + 1;
        }

        if (c) {
            return c;
        }
    }
}
