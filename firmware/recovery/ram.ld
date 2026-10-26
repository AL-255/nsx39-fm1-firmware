/* SPDX-License-Identifier: GPL-3.0-only */
MEMORY { SRAM (rwx): ORIGIN = 0x01c02000, LENGTH = 4096 }
SECTIONS {
 .text : { KEEP(*(.text.entry)) *(.text*) *(.rodata*) } > SRAM
 .data : { *(.data*) *(.sdata*) } > SRAM
 .bss (NOLOAD) : { *(.bss*) *(.sbss*) *(COMMON) } > SRAM
 /DISCARD/ : { *(.comment) *(.note*) }
}
