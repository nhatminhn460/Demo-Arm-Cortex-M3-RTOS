ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K 
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS
{
    /* 1. Code và Vector Table nằm trong Flash */
    .text :
    {
        KEEP(*(.isr_vector))  /* Giữ bảng vector ở đầu */
        *(.text*)             /* Code chương trình */
        *(.rodata*)           /* Dữ liệu hằng số (const) */
        . = ALIGN(4);
        _etext = .;           /* Đánh dấu cuối vùng code */
    } > FLASH

    /* 2. Dữ liệu khởi tạo (.data) */
    /* AT(_etext) nghĩa là: Lưu ở Flash ngay sau _etext, nhưng chạy ở RAM */
    _sidata = LOADADDR(.data); 

    .data : 
    {
        . = ALIGN(4);
        _sdata = .;        /* Bắt đầu vùng data trong RAM */
        *(.data*)
        . = ALIGN(4);
        _edata = .;        /* Kết thúc vùng data trong RAM */
    } > RAM AT > FLASH

    /* 3. Dữ liệu chưa khởi tạo (.bss) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;         /* Bắt đầu vùng bss */
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;         /* Kết thúc vùng bss */
    } > RAM

    /* 4. Stack Pointer */
    /* Đặt đỉnh Stack ở cuối RAM */
    _estack = ORIGIN(RAM) + LENGTH(RAM);
}
