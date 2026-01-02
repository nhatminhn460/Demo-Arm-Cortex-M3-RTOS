.syntax unified // GNU(trình biên dịch assembly) bật UAL(cú pháp thống nhất do arm đưa ra)
.cpu cortex-m3 // tên chip là cortex m3 -> cho biết đúng kiến trúc cpu để kiểm tra tập lệnh, encoding và tối ưu mã máy
.thumb //  sử dụng tập lệnh thumb (16 bit) 

/* ========================================
   PHẦN 1: KHAI BÁO GLOBAL & EXTERN
   ======================================== */
   // global là được dùng ở file khác có thể truy cập được, được linker sử dụng để liên kết các tệp khác nhau lại với nhau
   // nếu không khai báo global thì linker không tùn thấy thì vector table không được đưa vào đúng địa chỉ
.global vector_table // Định nghĩa vector_table là một biểu tượng toàn cục
.global Reset_Handler // Định nghĩa Reset_Handler là một biểu tượng toàn cục

.extern _estack // đỉnh của stack - thường là cuối RAM
.extern _sidata   /* Đầu dữ liệu trong Flash */
.extern _sdata    /* Đầu dữ liệu trong RAM */
.extern _edata    /* Cuối dữ liệu trong RAM */
.extern _sbss     /* Đầu vùng BSS */
.extern _ebss     /* Cuối vùng BSS */

.extern main  // nói với assembly rằng hàm main() được định nghĩa ở C, rết handler sẽ gọi bl main
.extern PendSV_Handler
.extern SysTick_Handler
.extern start_first_task // hàm khởi tạo task đầu tiên trong hệ điều hành thời gian thực

/* ========================================
   PHẦN 2: VECTOR TABLE
   ======================================== */
.section .isr_vector, "a", %progbits // đưa phần mã tiếp theo vào section tên .isr_vector, với thuộc tính "a" (allocatable -> section này sẽ được đưa vào falsh bởi linker) và loại %progbits (dữ liệu chương trình)
.type vector_table, %object // định nghĩa vector_table là một đối tượng (object)

vector_table:
    .word _estack // entry0: giá trị này dùng để nạp vào main stack pointer cpu đọc entry này ngay sau reset
    .word Reset_Handler  // entry1: địa chỉ của hàm reset handler CPU nhảy tới đây sau khi thiết lập MSP
    .word Default_Handler    /* NMI */
    .word Default_Handler    /* HardFault */
    .word MemManage_Handler  /* MemManage */
    .word Default_Handler    /* BusFault */
    .word Default_Handler    /* UsageFault */
    .word 0,0,0,0            /* Reserved */
    .word Default_Handler    /* SVC */
    .word Default_Handler    /* DebugMon */
    .word 0                  /* Reserved */
    .word PendSV_Handler     /* PendSV */ // dùng cho context switch
    .word SysTick_Handler    /* SysTick */ //timer tick của cortex - M3

    .word Default_Handler /* IRQ0 : GPIO port A */
    .word Default_Handler /* IRQ1 : GPIO port B */
    .word Default_Handler /* IRQ2 : GPIO port C */
    .word Default_Handler /* IRQ3 : GPIO port D */
    .word Default_Handler /* IRQ4 : GPIO port E */
    .word UART0_Handler /* IRQ5 : UART0 */
    
    .rept 43 // lặp lại 43 lần
        .word Default_Handler
    .endr

/* ========================================
   PHẦN 3: RESET HANDLER : copy data từ flash sang ram , xóa vùng bss và gọi main
   ======================================== */
.weak MemManage_Handler
.thumb_set MemManage_Handler, Default_Handler

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    /* 1. Copy .data từ Flash sang RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b loop_copy_data

copy_data:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

loop_copy_data:
    adds r4, r0, r3
    cmp r4, r1
    bcc copy_data

    /* 2. Xóa .bss về 0 */
    ldr r2, =_sbss
    ldr r4, =_ebss
    movs r3, #0
    b loop_zero_bss

zero_bss:
    str r3, [r2]
    adds r2, r2, #4

loop_zero_bss:
    cmp r2, r4
    bcc zero_bss
    bl mpu_init
    /* 3. Vào Main */
    bl main
    b .

/* Default Handler */
.section .text.Default_Handler
.weak Default_Handler
.type Default_Handler, %function
Default_Handler: // phần interrupt chưa được đinh nghĩa
    b .
    