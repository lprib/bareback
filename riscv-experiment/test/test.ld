ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text : { *(.text*) }

    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    .bss : { *(.bss*) *(COMMON) }

    /*
    . = ALIGN(16);
    _stack_bottom = .;
    . += 0x4000;
    _stack_top = .;
    */
}
