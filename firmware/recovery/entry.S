/* SPDX-License-Identifier: GPL-3.0-only */
.section .text.entry,"ax",@progbits
.globl _start
_start:
    cli
    sp = 0x01c7a000
    ssp = 0x01c7c000
    call recover_boot_state
1:  goto 1b
