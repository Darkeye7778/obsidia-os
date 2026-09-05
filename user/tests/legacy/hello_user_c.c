/* hello_user_c.c
 * C port of the original hello_user.asm flat binary demo.
 * Build: make userland/hello_user_c.bin
 * It should behave identically when run via the shell "run" command.
 */

#include "syscall.h"

static const char msg[] = "Hello from userland\n";

void _start(void) {
    /* print hello (matches original: buf + len without the null) */
    sys_write(msg, sizeof(msg) - 1);

    /* get ticks (value ignored, same as asm demo) */
    (void)sys_getticks();

    /* yield a few times */
    for (int i = 0; i < 3; i++) {
        sys_yield();
    }

    /* exit cleanly */
    sys_exit(0);

    /* shouldn't reach */
    for (;;) { }
}
