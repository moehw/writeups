OUTPUT_FORMAT(binary)
OUTPUT_ARCH(i386)

SECTIONS {
  . = 0x7fbd8a4;
  .text : {
    *(.text)
  }
  .data : { 
    *(.data)
    *(.rodata)
  }
  .bss : { 
    *(.bss) 
  }
}