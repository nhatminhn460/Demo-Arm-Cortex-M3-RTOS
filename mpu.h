#ifndef MPU_H
#define MPU_H

#include <stdint.h>
#include "process.h"
#include "uart.h"


struct PCB_t; // Forward declaration of PCB_t

/* MPU Registers */
#define MPU_BASE    0xE000ED90 // địa chỉ cơ sở của MPU
#define MPU_TYPE    (*(volatile uint32_t*)(MPU_BASE + 0x00)) // thanh ghi loại MPU
#define MPU_CTRL    (*(volatile uint32_t*)(MPU_BASE + 0x04)) // thanh ghi điều khiển MPU
#define MPU_RNR     (*(volatile uint32_t*)(MPU_BASE + 0x08)) // thanh ghi số vùng MPU hiện tại
#define MPU_RBAR    (*(volatile uint32_t*)(MPU_BASE + 0x0C)) // thanh ghi cơ sở vùng MPU
#define MPU_RASR    (*(volatile uint32_t*)(MPU_BASE + 0x10)) // thanh ghi thuộc tính vùng MPU

/* MPU Control Register */
#define MPU_CTRL_ENABLE_Msk     (1UL << 0) // Bit kích hoạt MPU
#define MPU_CTRL_HFNMIENA_Msk   (1UL << 1) // Bit cho phép bảo vệ khi vào NMI và HardFault
#define MPU_CTRL_PRIVDEFENA_Msk (1UL << 2) // Bit cho phép vùng mặc định cho chế độ ưu tiên

/* MPU Region Attribute and Size Register */
#define MPU_RASR_ENABLE_Pos     0 // Bit kích hoạt vùng MPU
#define MPU_RASR_SIZE_Pos       1 // Vị trí kích thước vùng MPU
#define MPU_RASR_SRD_Pos        8 // Vị trí byte bảo vệ con
#define MPU_RASR_B_Pos          16 // Bit bộ nhớ bộ đệm 
#define MPU_RASR_C_Pos          17 // Bit bộ nhớ đệm 
#define MPU_RASR_S_Pos          18 // Bit chia sẻ
#define MPU_RASR_TEX_Pos        19 // Vị trí loại vùng bộ nhớ
#define MPU_RASR_AP_Pos         24 // Vị trí quyền truy cập 
#define MPU_RASR_XN_Pos         28 // Bit không thực thi

/* System Control Block */
#define SCB_BASE                0xE000ED00 // địa chỉ cơ sở của SCB
#define SCB_SHCSR               (*(volatile uint32_t*)(SCB_BASE + 0x24)) // thanh ghi kiểm soát và trạng thái hệ thống
#define SCB_CFSR                (*(volatile uint32_t*)(SCB_BASE + 0x28)) // thanh ghi lỗi bảo vệ bộ nhớ
#define SCB_MMFAR               (*(volatile uint32_t*)(SCB_BASE + 0x34)) // thanh ghi địa chỉ lỗi bộ nhớ

#define SCB_SHCSR_MEMFAULTENA_Msk   (1UL << 16) // Bit cho phép ngắt lỗi bộ nhớ
#define SCB_CFSR_MEMFAULTSR_Msk     (0xFFUL) // Mặt nạ lỗi bảo vệ bộ nhớ

/* Memory barrier instructions */
#define __DSB() __asm volatile ("dsb" : : : "memory")
#define __ISB() __asm volatile ("isb" : : : "memory")

/* Helper function to calculate MPU region size value */
static inline uint32_t mpu_calc_region_size(uint32_t size)
{
    /* SIZE field = log2(size) - 1 
     * Minimum: 32 bytes (value 4)
     * Example: 1KB = 2^10 -> value = 9
     */
    uint32_t value = 0;
    size = size >> 1;
    while (size) 
    {
        value++;
        size >>= 1;
    }
    return value - 1;
}

void mpu_init(void);
void mpu_config_for_task(PCB_t *task);

#endif