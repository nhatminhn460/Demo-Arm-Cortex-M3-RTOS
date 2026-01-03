#include "mpu.h"
#define SCB_ICSR (*(volatile uint32_t*)0xE000ED04)
#define PENDSVSET_BIT (1UL << 28)

void mpu_init(void)
{
    /* Disable MPU */
    MPU_CTRL = 0;

    /* ===== REGION 0: KERNEL (privileged only) ===== */
    MPU_RNR  = 0;
    MPU_RBAR = 0x00000000;  /* Flash alias trên QEMU */
    MPU_RASR =
        (0 << MPU_RASR_XN_Pos) |      /* Executable */
        (2 << MPU_RASR_AP_Pos) |      /* AP=010: Priv RW, User RO */
        (0 << MPU_RASR_TEX_Pos) |     /* Normal memory */
        (1 << MPU_RASR_C_Pos) |       /* Cacheable */
        (0 << MPU_RASR_B_Pos) |       /* Not bufferable */
        (0 << MPU_RASR_S_Pos) |       /* Not shareable */
        (17 << MPU_RASR_SIZE_Pos) |   /* 256KB (2^18) → SIZE=17 */
        (1 << MPU_RASR_ENABLE_Pos);   /* Enable region */
    
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
    
     /* REGION 5: Flash mirror tại 0x08000000 (optional) */
    MPU_RNR  = 5;
    MPU_RBAR = 0x08000000;
    MPU_RASR =
        (0 << MPU_RASR_XN_Pos) |
        (2 << MPU_RASR_AP_Pos) |
        (0 << MPU_RASR_TEX_Pos) |
        (1 << MPU_RASR_C_Pos) |
        (0 << MPU_RASR_B_Pos) |
        (0 << MPU_RASR_S_Pos) |
        (17 << MPU_RASR_SIZE_Pos) |
        (1 << MPU_RASR_ENABLE_Pos);
    
    uart_print("  Region 5 (Flash mirror): 0x08000000, 256KB\r\n");


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

    uint8_t mmfsr = SCB_CFSR & 0xFF;
    uart_print("MMFSR: 0x");
    uart_print_hex(mmfsr);
    uart_print("\r\n");

    if (mmfsr & (1 << 7))  // MMARVALID
    {
        uint32_t fault_addr = SCB_MMFAR;
        uart_print("Fault Addr: 0x");
        uart_print_hex32(fault_addr);
        uart_print("\r\n");
    }

    /* Clear fault flags */
    SCB_CFSR |= 0xFF;

    /* Suspend faulting task */
    if (current_pcb)
    {
        current_pcb->state = PROC_SUSPENDED;
        uart_print("Task suspended\r\n");
        current_pcb = NULL;
    }

    SCB_ICSR |= PENDSVSET_BIT;    
    return;
}