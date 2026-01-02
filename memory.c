#include "memory.h"
#include "mpu.h"
#include "process.h"

static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(4096))); // aligned(8) đảm bảo mảng này bắt đầu ở địa chỉ chia hết cho 8
static mem_block_t *free_list = NULL; // con trỏ đầu danh sách

void os_mem_init(void) {
    free_list = (mem_block_t *)heap_area;
    free_list->next = NULL;
    free_list->size = HEAP_SIZE - sizeof(mem_block_t);
    free_list->is_free = 1;
}

void* os_malloc(size_t size) 
{
    return os_malloc_aligned(size, 8);
}

void* os_malloc_aligned(size_t size, size_t alignment) {
    if (alignment < 8) alignment = 8;
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;  /* alignment phải là power of 2 */
    }

    void *ptr = NULL;
    size = (size + 7) & ~0x07;  /* Round up to 8 bytes */

    OS_ENTER_CRITICAL();
    
    mem_block_t *current = free_list;
    while (current) {
        if (current->is_free && current->size >= size) {
            /* Tính địa chỉ data sau header */
            uintptr_t data_addr = (uintptr_t)current + sizeof(mem_block_t);
            
            /* Tính offset cần thiết để align */
            uintptr_t aligned_addr = (data_addr + alignment - 1) & ~(alignment - 1);
            size_t padding = aligned_addr - data_addr;
            
            /* Kiểm tra có đủ chỗ cho padding + size không */
            if (current->size >= size + padding) {
                /* Nếu có padding, tạo dummy block */
                if (padding >= sizeof(mem_block_t) + 8) {
                    mem_block_t *padding_block = current;
                    mem_block_t *aligned_block = (mem_block_t*)(aligned_addr - sizeof(mem_block_t));
                    
                    padding_block->size = padding - sizeof(mem_block_t);
                    padding_block->is_free = 0;  /* Đánh dấu used để không merge */
                    padding_block->next = aligned_block;
                    
                    aligned_block->size = current->size - padding;
                    aligned_block->is_free = 1;
                    aligned_block->next = current->next;
                    
                    current = aligned_block;
                    data_addr = aligned_addr;
                } else if (padding > 0) {
                    /* Padding quá nhỏ, tìm block khác */
                    current = current->next;
                    continue;
                }
                
                /* Split block nếu còn dư */
                if (current->size > size + sizeof(mem_block_t) + 8) {
                    mem_block_t *new_block = (mem_block_t*)((uint8_t*)current + sizeof(mem_block_t) + size);
                    new_block->size = current->size - size - sizeof(mem_block_t);
                    new_block->is_free = 1;
                    new_block->next = current->next;
                    current->size = size;
                    current->next = new_block;
                }
                
                current->is_free = 0;
                ptr = (void*)data_addr;
                break;
            }
        }
        current = current->next;
    }
    
    OS_EXIT_CRITICAL();
    return ptr;
}

void os_free(void *ptr) {
    if (ptr == NULL) return;

    OS_ENTER_CRITICAL();

    mem_block_t *block = (mem_block_t*)((uint8_t*)ptr - sizeof(mem_block_t));
    block->is_free = 1;

    mem_block_t *current = free_list;
    while (current && current->next != block) 
    {
        current = current->next;
    }
    if (block->next && block->next->is_free) 
    {
        block->size += sizeof(mem_block_t) + block->next->size;
        block->next = block->next->next;
    }
    
    OS_EXIT_CRITICAL();
}

uint32_t mpu_calc_alignment(size_t size) 
{
    uint32_t size_bits = mpu_calc_region_size(size);
    return 1U << (size_bits + 1);
}