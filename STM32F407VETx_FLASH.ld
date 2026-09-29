/* STM32F407VE: 512 KiB Flash, 128 KiB SRAM, separate 64 KiB CCM.
 * CCM is unused: it is not contiguous with SRAM or DMA-accessible.
 * Symbols match ST's unmodified startup_stm32f407xx.s.
 */
ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  CCM   (xrw) : ORIGIN = 0x10000000, LENGTH = 64K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 0x800;
PHDRS
{
  flash PT_LOAD FLAGS(5); /* R-X */
  ram PT_LOAD FLAGS(6);   /* RW- */
  zero PT_LOAD FLAGS(6);  /* zero-initialized SRAM, no Flash payload */
}
SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH :flash
  .text :
  {
    . = ALIGN(4);
    *(.text .text.*) *(.glue_7) *(.glue_7t)
    KEEP(*(.init)) KEEP(*(.fini))
    . = ALIGN(4); _etext = .;
  } > FLASH :flash
  .rodata : { . = ALIGN(4); *(.rodata .rodata.*) . = ALIGN(4); } > FLASH :flash
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH :flash
  .ARM.exidx :
  {
    __exidx_start = .; *(.ARM.exidx* .gnu.linkonce.armexidx.*) __exidx_end = .;
  } > FLASH
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4); _sdata = .; *(.data .data.*) . = ALIGN(4); _edata = .;
  } > RAM AT > FLASH :ram
  .bss (NOLOAD) : AT(ADDR(.bss))
  {
    . = ALIGN(4); _sbss = .; __bss_start__ = .;
    *(.bss .bss.*) *(COMMON)
    . = ALIGN(4); _ebss = .; __bss_end__ = .;
  } > RAM :zero
  .stack_reserve (NOLOAD) : AT(ADDR(.stack_reserve))
  {
    . = ALIGN(8); _end = .; PROVIDE(end = .);
    . += _Min_Stack_Size;
  } > RAM :zero
  .ARM.attributes 0 : { *(.ARM.attributes) }
  ASSERT(_end + _Min_Stack_Size <= _estack, "SRAM overflow: no room for stack")
}
