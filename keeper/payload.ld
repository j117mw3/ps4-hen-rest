/* Based on the pinned PS4 Payload SDK libPS4/linker.x. BSS bounds make each
 * execution initialize its own SDK pointers, including a reused payload buffer. */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
PHDRS { code_seg PT_LOAD; rdata_seg PT_LOAD; data_seg PT_LOAD; bss_seg PT_LOAD; }
SECTIONS {
    .text : { *(.text.start) *(.text*) } : code_seg
    .rodata : { *(.rodata*) } : rdata_seg
    .data : { *(.data*) } : data_seg
    .bss : { __payload_bss_start = .; *(.bss*) *(COMMON) __payload_bss_end = .; } : bss_seg
    /DISCARD/ : { *(.comment) *(.note.GNU-stack) *(.eh_frame) *(.interp) }
}
