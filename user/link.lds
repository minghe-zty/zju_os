ENTRY(_start)

SECTIONS
{
    . = 0x0;
    _start = .;

    .text.init : { 
        *(.text.init) 
    }
    
    .text : { 
        *(.text) 
    }
    
    .data : { 
        *(.data) 
        *(.rodata) 
        *(.sbss) 
        *(.bss) 
        *(.data.*) 
        *(.rodata.*) 
        *(.sbss.*) 
        *(.bss.*) 
    }
}
