// SPDX-License-Identifier: GPL-3.0-only
// Temporary SRAM-only recovery for the faulty FM-1_939 boot marker.
// No flash/NOR/SFC/USB driver is linked. Clear retained state, then core-reset.
#include <stdint.h>
#define FM1_CPU1_USED 1
#include "fm1_sys.h"
__attribute__((noreturn)) void recover_boot_state(void)
{
    __asm__ volatile("cli" ::: "memory");
    fm1_cpu1_halt();
    // Reopen the mailbox in case the previous firmware left write guards set.
    if (!(*(volatile uint32_t *)0x01eee240u & 1u))
        *(volatile uint32_t *)0x01eee240u = 0xe7u;
    *(volatile uint32_t *)0x01eee348u = 0;
    *(volatile uint32_t *)0x01c7c040u = 0; // old boot_magic
    *(volatile uint32_t *)0x01c7c044u = 0; // old boot_attempts
    fm1_mailbox_clear();                  // clear the USB-update request
    __asm__ volatile("csync" ::: "memory");
    fm1_core_reset();                     // reboot through the existing ROM/SPL
    for (;;)
        ;
}
