#include "mpu.h"

void mpu_init(void)
{
    /* Disable MPU */
    MPU_CTRL = 0;

    /* ===== REGION 0: KERNEL (privileged only) ===== */
    MPU_RNR  = 0;
    MPU_RBAR = 0x08000000;  // Flash base address
    /* REGION 0: KERNEL (Flash) - Code readable by all */
    MPU_RASR =
        (0 << MPU_RASR_XN_Pos) |      // Executable
        (2 << MPU_RASR_AP_Pos) |      // AP=010: Privileged RW, User Read-Only ✅
        (0 << MPU_RASR_TEX_Pos) |     
        (1 << MPU_RASR_C_Pos) |       
        (0 << MPU_RASR_B_Pos) |       
        (0 << MPU_RASR_S_Pos) |       
        (18 << MPU_RASR_SIZE_Pos) |   
        (1 << MPU_RASR_ENABLE_Pos);
    
    /* REGION 3: Peripherals (Device memory) */
    MPU_RNR  = 3;
    MPU_RBAR = 0x40000000;  // Peripheral base
    MPU_RASR =
        (1 << MPU_RASR_XN_Pos) |      // No Execute
        (3 << MPU_RASR_AP_Pos) |      // Full access
        (0 << MPU_RASR_TEX_Pos) |     // Device memory
        (0 << MPU_RASR_C_Pos) |       
        (1 << MPU_RASR_B_Pos) |       // Bufferable for device
        (1 << MPU_RASR_S_Pos) |       
        (28 << MPU_RASR_SIZE_Pos) |   // 512MB (covers all peripherals)
        (1 << MPU_RASR_ENABLE_Pos);

    /* REGION 4: System Control Space */
    MPU_RNR  = 4;
    MPU_RBAR = 0xE0000000;  // PPB (Private Peripheral Bus)
    MPU_RASR =
        (1 << MPU_RASR_XN_Pos) |
        (3 << MPU_RASR_AP_Pos) |
        (0 << MPU_RASR_TEX_Pos) |
        (0 << MPU_RASR_C_Pos) |
        (1 << MPU_RASR_B_Pos) |
        (1 << MPU_RASR_S_Pos) |
        (28 << MPU_RASR_SIZE_Pos) |
        (1 << MPU_RASR_ENABLE_Pos);

    /* Enable MemManage fault */
    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;

    /* Enable MPU with default memory map for privileged access */
    MPU_CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    
    __DSB();
    __ISB();
}

void mpu_config_for_task(PCB_t *task)
{
    /* Disable MPU during reconfiguration */
    MPU_CTRL = 0;
    __DSB();

    uint32_t stack_size_bits = mpu_calc_region_size(task->stack_size);
    uint32_t required_alignment = 1U << (stack_size_bits + 1);
    if ((uint32_t)task->stack_base % required_alignment != 0)
    {
        uart_print("ERROR: Stack base not aligned!\r\n");
        return;
    }

    /* ===== REGION 1: Task Stack ===== */
    MPU_RNR = 1;
    MPU_RBAR = (uint32_t)task->stack_base;
    MPU_RASR =
        (1 << MPU_RASR_XN_Pos) |      // No execute (stack should not be executable)
        (3 << MPU_RASR_AP_Pos) |      // Full access (RW)
        (1 << MPU_RASR_TEX_Pos) |     // Normal memory
        (0 << MPU_RASR_C_Pos) |       
        (1 << MPU_RASR_B_Pos) |       
        (0 << MPU_RASR_S_Pos) |       // Unshareable
        (stack_size_bits << MPU_RASR_SIZE_Pos) |
        (1 << MPU_RASR_ENABLE_Pos);

    /* ===== REGION 2: Task Heap/Data ===== */
    if (task->heap_base && task->heap_size > 0)
    {
        MPU_RNR = 2;
        MPU_RBAR = task->heap_base;
        
        uint32_t heap_size_bits = mpu_calc_region_size(task->heap_size);
        MPU_RASR =
            (1 << MPU_RASR_XN_Pos) |  // No execute
            (3 << MPU_RASR_AP_Pos) |  // Full access (RW)
            (1 << MPU_RASR_TEX_Pos) | // Normal memory
            (1 << MPU_RASR_C_Pos) |   
            (1 << MPU_RASR_B_Pos) |   
            (0 << MPU_RASR_S_Pos) |   
            (heap_size_bits << MPU_RASR_SIZE_Pos) |
            (1 << MPU_RASR_ENABLE_Pos);
    }

    /* Re-enable MPU */
    MPU_CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    
    __DSB();  // Data Synchronization Barrier
    __ISB();  // Instruction Synchronization Barrier
}

void MemManage_Handler(void)
{
    uart_print("\r\n*** MPU FAULT ***\r\n");

    if (current_pcb)
    {
        uart_print("Task ID: ");
        uart_print_dec(current_pcb->pid);
        uart_print("\r\n");
    }

    /* Read MMFSR (MemManage Fault Status Register) */
    uint8_t mmfsr = SCB_CFSR & 0xFF;
    uart_print("MMFSR: 0x");
    uart_print_hex(mmfsr);
    uart_print("\r\n");

    /* Check if MMFAR is valid */
    if (mmfsr & (1 << 7))  // MMARVALID bit
    {
        uint32_t fault_addr = SCB_MMFAR;
        uart_print("Fault Addr: 0x");
        uart_print_hex32(fault_addr);
        uart_print("\r\n");
    }

    /* Clear MemManage fault flags */
    SCB_CFSR |= 0xFF;  // Clear all MemManage flags

    /* Kill or suspend the faulting task */
    if (current_pcb)
    {
        current_pcb->state = PROC_SUSPENDED;
        uart_print("Task suspended\r\n");
    }

    /* Trigger scheduler to switch to another task */
    process_schedule();

    /* Should not reach here if scheduler works properly */
    while (1);
}