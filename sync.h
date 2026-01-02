#ifndef SYNC_H
#define SYNC_H

#include "process.h"

/* --- 1. SEMAPHORE --- */
typedef struct {
    int32_t count;      // Số lượng tài nguyên
    int32_t max_count;  // Giới hạn tối đa
    queue_t wait_list;  // Danh sách các task đang đợi
} os_sem_t;

void sem_init(os_sem_t *sem, int32_t initial_count);
void sem_wait(os_sem_t *sem);
void sem_signal(os_sem_t *sem);

/* --- 2. MUTEX --- */
typedef struct {
    int locked;         // 0: Mở, 1: Khóa
    PCB_t *owner;       // Ai đang giữ khóa? (Quan trọng cho Mutex)
    queue_t wait_list;  // Danh sách đợi
} os_mutex_t;

void mutex_init(os_mutex_t *mtx);
void mutex_lock(os_mutex_t *mtx);
void mutex_unlock(os_mutex_t *mtx);

#endif