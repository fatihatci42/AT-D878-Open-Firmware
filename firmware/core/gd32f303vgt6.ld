/*
 * AT-D878 Open Firmware
 * Linker Script - GD32F303VGT6
 *
 * Flash: 1 MB (0x08000000 - 0x080FFFFF)
 * SRAM:  96 KB (0x20000000 - 0x20017FFF)
 *
 * İlk 16 KB üretici bootloader'a ayrılmıştır.
 * Uygulama 0x08004000'den başlar.
 *
 * UYARI: Bu linker script henüz donanım üzerinde doğrulanmamıştır.
 * Bootloader boyutu ve flash bölgeleri gerçek cihazda farklılık gösterebilir.
 */

/* Entry point */
ENTRY(Reset_Handler)

/* Yığın boyutu (stack) */
_Min_Stack_Size = 0x1000;  /* 4 KB minimum stack */
_Min_Heap_Size  = 0x2000;  /* 8 KB minimum heap */

/* Bellek bölgeleri */
MEMORY
{
    /* Bootloader bölgesi - DOKUNMA */
    BOOTLOADER (rx)  : ORIGIN = 0x08000000, LENGTH = 16K

    /* Uygulama flash alanı */
    FLASH (rx)       : ORIGIN = 0x08004000, LENGTH = 944K

    /* Yapılandırma saklama alanı (flash'ın sonunda) */
    CONFIG (r)       : ORIGIN = 0x080F0000, LENGTH = 64K

    /* SRAM */
    RAM (xrw)        : ORIGIN = 0x20000000, LENGTH = 96K
}

/* Bölümler */
SECTIONS
{
    /* Vektör tablosu - Flash başlangıcında */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* Kod bölümü */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* Salt okunur veri */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* ARM unwinding tabloları */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* Preinit/init/fini dizileri */
    .preinit_array :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > FLASH

    /* Flash'ta başlangıç değerleri olan RAM verileri */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    /* Sıfırlanacak BSS bölümü */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    /* Heap ve stack kontrol bölümü */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE (end = .);
        PROVIDE (_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > RAM

    /* Yapılandırma verisi (flash'ın sonunda) */
    .config_data :
    {
        . = ALIGN(4);
        _sconfig = .;
        KEEP(*(.config_data))
        . = ALIGN(4);
        _econfig = .;
    } > CONFIG

    /* Debug bölümleri */
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}

/* Stack pointer başlangıç değeri (RAM'in en sonunda) */
_estack = ORIGIN(RAM) + LENGTH(RAM);
