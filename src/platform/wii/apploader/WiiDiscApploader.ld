OUTPUT_ARCH(powerpc:common)
ENTRY(ApploaderEntry)

SECTIONS
{
    . = 0x81200000;

    .text : ALIGN(4)
    {
        KEEP(*(.text.apploader_entry))
        *(.text*)
        *(.rodata*)
    }

    .data : ALIGN(4)
    {
        *(.data*)
    }

    /DISCARD/ :
    {
        *(.bss*)
        *(.sbss*)
        *(.sdata*)
        *(.sdata2*)
        *(.comment*)
        *(.eh_frame*)
        *(.gnu*)
        *(.ctors*)
        *(.dtors*)
    }
}
