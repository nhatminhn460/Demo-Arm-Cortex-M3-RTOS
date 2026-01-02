#include "sync.h"

/* ============================================================
   HÀM NỘI BỘ (STATIC) - Dùng chung cho cả 2 để giảm lặp code
   ============================================================ */
// hàm khởi tạo sem
void sem_init(os_sem_t* sem, int32_t initial_count){
    sem->count = initial_count;
    queue_init(&sem->wait_list);
}

// Hàm đưa task hiện tại vào danh sách chờ và gọi Scheduler
static void block_current_task(queue_t *wait_queue) {
    // Logic này giống hệt nhau ở cả Mutex và Semaphore!
    OS_ENTER_CRITICAL();
    
    current_pcb->state = PROC_BLOCKED;
    queue_enqueue(wait_queue, current_pcb);
    
    OS_EXIT_CRITICAL();
    
    process_schedule(); // Chuyển sang task khác
}

// Hàm đánh thức task đang chờ
static void wake_up_waiting_task(queue_t *wait_queue) {
    OS_ENTER_CRITICAL();
    if (!queue_is_empty(wait_queue)) {
        PCB_t *t = queue_dequeue(wait_queue);
        t->state = PROC_READY;
        
        // SỬA: Thay queue_enqueue bằng hàm thêm vào hàng đợi ưu tiên
        add_task_to_ready_queue(t); 
        
        /* TÙY CHỌN: Preemption (Ngắt quãng)
           Nếu task vừa được đánh thức có độ ưu tiên cao hơn task đang chạy,
           ta nên kích hoạt Scheduler ngay lập tức để nó chiếm quyền CPU.
        */
        if (current_pcb && t->dynamic_priority > current_pcb->dynamic_priority) {
             *(volatile uint32_t*)0xE000ED04 |= (1UL << 28); // Trigger PendSV
        }
    }
    OS_EXIT_CRITICAL();
}

/* ============================================================
   PHẦN SEMAPHORE
   ============================================================ */
void sem_wait(os_sem_t *sem) {
    while (1) { // Vòng lặp để kiểm tra lại sau khi thức dậy
        OS_ENTER_CRITICAL();
        if (sem->count > 0) {
            sem->count--;
            OS_EXIT_CRITICAL();
            return; // Lấy được rồi, thoát
        }
        OS_EXIT_CRITICAL();
        
        // Nếu chưa có, đi ngủ
        block_current_task(&sem->wait_list);
    }
}

void sem_signal(os_sem_t *sem) {
    OS_ENTER_CRITICAL();
    sem->count++;
    OS_EXIT_CRITICAL();
    
    wake_up_waiting_task(&sem->wait_list);
}

/* ============================================================
   PHẦN MUTEX (Logic hơi khác chút xíu về Owner)
   ============================================================ */
void mutex_init(os_mutex_t* mtx){
    mtx->locked = 0; //ban đầu không khóa
    mtx->owner = NULL; // chưa ai sở hữu 
    queue_init(&mtx->wait_list);
}

void mutex_lock(os_mutex_t *mtx) {
    while (1) {
        OS_ENTER_CRITICAL();
        if (mtx->locked == 0) {
            mtx->locked = 1;
            mtx->owner = current_pcb; // Ghi nhận chủ sở hữu
            OS_EXIT_CRITICAL();
            return;
        }
        OS_EXIT_CRITICAL();
        
        block_current_task(&mtx->wait_list);
    }
}

void mutex_unlock(os_mutex_t *mtx) {
    OS_ENTER_CRITICAL();
    // Chỉ chủ sở hữu mới được mở khóa (Tính năng riêng của Mutex)
    if (mtx->owner == current_pcb) {
        mtx->locked = 0;
        mtx->owner = NULL;
    }
    OS_EXIT_CRITICAL();
    
    wake_up_waiting_task(&mtx->wait_list);
}