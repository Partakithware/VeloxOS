OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x100000;
    
    .text : ALIGN(4096) {
        *(.text*)
    }
    
    .rodata : ALIGN(4096) {
        *(.rodata*)
    }
    
    .data : ALIGN(4096) {
        *(.data*)
    }
    
    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
    
    /DISCARD/ : {
        *(.note*)
        *(.comment)
        *(.eh_frame)
    }
}