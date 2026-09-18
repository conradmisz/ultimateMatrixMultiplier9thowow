MEMORY {
  instr (rwx) : ORIGIN = 0x00000000, LENGTH = 16K
  data  (rw)  : ORIGIN = 0x00010000, LENGTH = 8K
}

/* .data lives in the instruction RAM region: it is a writable RAM preloaded from the hex,
   so initialised globals need no copy loop. Stack and .bss live in data RAM. */
SECTIONS {
  .text : {
    KEEP(*(.text.start))
    *(.text .text.*)
  } > instr
  .rodata : { *(.rodata .rodata.* .srodata .srodata.*) } > instr
  .data   : { *(.data .data.* .sdata .sdata.*) . = ALIGN(4); } > instr
  .bss (NOLOAD) : {
    . = ALIGN(4);
    _bss_start = .;
    *(.bss .bss.* .sbss .sbss.* COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > data
  _stack_top = ORIGIN(data) + LENGTH(data);
  /DISCARD/ : { *(.eh_frame) *(.comment) *(.note*) }
}
