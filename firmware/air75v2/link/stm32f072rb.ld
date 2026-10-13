/* STM32F072RB: 128 KB flash, 16 KB RAM. The top 32 KB of flash (16 x 2 KB pages) hold the wear-levelled settings store: two 16 KB banks. */
ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 96K
  CONFIG (r)  : ORIGIN = 0x08018000, LENGTH = 32K
  RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 16K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
__config_base = ORIGIN(CONFIG);
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  _sidata = LOADADDR(.data);
  .data : { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss  : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  .noinit (NOLOAD) : { *(.noinit*) } > RAM
  /* the stack grows down from _estack: refuse to link if statics leave it less than 2 KB */
  _min_stack = 0x800;
  ASSERT(. + _min_stack <= _estack, "RAM overflow: less than 2 KB left for the stack")
  /DISCARD/ : { *(.ARM.attributes*) }
}
