#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h> //  for create size_t

#define HEAP_SIZE (32 * 1024)

typedef struct mem_block {
    struct mem_block * next;
    size_t size;
    uint8_t is_free;
} mem_block_t;

void os_mem_init(void); // create heap
void* os_malloc(size_t size);
void* os_malloc_aligned(size_t size, size_t alignment);
void os_free(void *ptr);
uint32_t mpu_calc_alignment(size_t size);
size_t os_get_free_heap_size(void);

#endif