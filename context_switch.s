.syntax unified
.cpu cortex-m3
.thumb

.global PendSV_Handler
.global start_first_task

/* External variables from C */
.extern current_pcb
.extern next_pcb

.section .text

/* ========================================
   HÀM: PendSV_Handler
   Mô tả: Thực hiện lưu và khôi phục ngữ cảnh (Context Switch)
   ======================================== */
.type PendSV_Handler, %function
PendSV_Handler:
    /* 0. Disable MPU temporarily (prevents faults during switch) */
    LDR     r3, =0xE000ED94         /* MPU_CTRL address */
    LDR     r2, [r3]
    PUSH    {r2}                    /* Save MPU_CTRL state */
    MOVS    r1, #0
    STR     r1, [r3]                /* MPU_CTRL = 0 (disable) */
    DSB
    
    /* 1. Lấy PSP hiện tại */
    MRS     r0, psp
    CBZ     r0, load_next_task

    /* 2. Lưu Context cũ */
    LDR     r1, =current_pcb
    LDR     r1, [r1]
    CBZ     r1, load_next_task
    
    STMDB   r0!, {r4-r11}
    STR     r0, [r1]

load_next_task:
    /* 3. Lấy Task tiếp theo */
    LDR     r1, =next_pcb
    LDR     r1, [r1]
    CBZ     r1, restore_mpu_and_exit
    
    PUSH    {r1}                    /* Save next_pcb */
    LDR     r4, [r1]                /* r4 = next_pcb->stack_ptr */
    
    /* 4. Config MPU cho task mới */
    MOV     r0, r1
    BL      mpu_config_for_task     /* MPU re-enabled inside this function */
    
    POP     {r1}
    
    /* 5. Cập nhật current_pcb */
    LDR     r2, =current_pcb
    STR     r1, [r2]

    /* 6. Khôi phục Context */
    MOV     r0, r4
    LDMIA   r0!, {r4-r11}
    MSR     psp, r0
    
    DSB
    ISB

pend_exit:
    ORR     lr, lr, #0x04
    BX      lr

restore_mpu_and_exit:
    /* Restore MPU nếu không có task */
    POP     {r2}
    LDR     r3, =0xE000ED94
    STR     r2, [r3]
    DSB
    ISB
    B       pend_exit

/* ========================================
   HÀM: start_first_task
   Mô tả: Khởi động task đầu tiên.
   Input: r0 = Stack Pointer ban đầu của task (đang trỏ vào đáy Fake Context)
   ======================================== */
.type start_first_task, %function
start_first_task:
    /* Input: r0 = PCB pointer (NOT stack_ptr!) */
    /* 1. Config MPU cho task đầu tiên */
    PUSH    {r0, lr}
    BL      mpu_config_for_task
    POP     {r0, lr}

    /* 2. Load stack pointer */
    LDR     r1, [r0]                /* r1 = task->stack_ptr */
    ADD     r1, r1, #32             /* Skip fake R4-R11 (8*4 = 32 bytes) */
    MSR     psp, r1

    /* 3. Switch to Unprivileged + PSP */
    MOVS    r0, #3                  /* Bit[1]=1 (PSP), Bit[0]=1 (Unprivileged) */
    MSR     CONTROL, r0
    ISB

    /* 4. Return to Thread mode với PSP */
    LDR     lr, =0xFFFFFFFD
    BX      lr
