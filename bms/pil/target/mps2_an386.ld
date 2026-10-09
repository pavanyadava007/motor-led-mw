/* QEMU mps2-an386 (Cortex-M4): code in SSRAM1 @0x0, data in SSRAM2 @0x20000000.
 * LENGTHs are sized like a mid-range automotive MCU (1 MiB code, 256 KiB RAM) so size overflows fail the link. */
ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1024K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}
_stack_size = 0x4000;
SECTIONS
{
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  _sidata = LOADADDR(.data);
  .data : { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss (NOLOAD) : { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  .stack (NOLOAD) : { . = ALIGN(8); . += _stack_size; _estack = .; } > RAM
}
