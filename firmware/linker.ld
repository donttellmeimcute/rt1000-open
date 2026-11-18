/* Mapa de memoria verificado por ingeniería inversa del HEX V1.16.
 * Flash total 256 KiB. Bootloader de fábrica 0x08000000-0x080027FF:
 * NUNCA incluirlo en el linker de la app.
 * App: 0x08002800, longitud 246 KiB (0x3D800).
 * RAM: stack top observado 0x2000AEC8 en V1.16; el tamaño exacto de
 * SRAM depende del MCU (por confirmar). Se reserva 48 KiB como
 * hipótesis de trabajo, NO como dato verificado.
 */
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08002800, LENGTH = 246K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 48K
}

_estack = 0x2000AEC8;

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data : AT(LOADADDR(.text) + SIZEOF(.text))
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM
  _sidata = LOADADDR(.data);

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM
  _end = _ebss;
  PROVIDE(end = _ebss);

  /* Secciones de unwind de ARM: no se usan en bare metal,
   * pero el linker las genera; descartarlas evita el solape
   * con .data. */
  /DISCARD/ :
  {
    *(.ARM.exidx*)
    *(.ARM.extab*)
  }
}
