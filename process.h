#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include "queue.h"
#include "banker.h"

#define MAX_PROCESSES 10 // Số lượng tiến trình tối đa
#define MAX_PRIORITY 8 // số hàng đợi tối đa
#define STACK_SIZE 256 // Kích thước stack cho mỗi tiến trình

// Lệnh Assembly để tắt ngắt (Set PRIMASK = 1)
#define OS_ENTER_CRITICAL()  __asm volatile ("cpsid i" : : : "memory")
// Lệnh Assembly để bật lại ngắt (Set PRIMASK = 0)
#define OS_EXIT_CRITICAL()   __asm volatile ("cpsie i" : : : "memory")

extern queue_t ready_queue[MAX_PRIORITY]; // mảng hàng đợi
extern queue_t job_queue; // Hàng đợi công việc (nếu cần, có thể bỏ qua nếu không dùng)
extern queue_t device_queue; // Hàng đợi công việc và thiết bị (nếu cần)
extern struct PCB* current_pcb; // PCB hiện tại
extern volatile uint32_t tick_count; // Biến đếm tick hệ thống
extern uint32_t top_ready_priority_bitmap; // ví dụ = 3 => 0000 1000

typedef enum {
    PROC_NEW,
    PROC_READY,
    PROC_RUNNING,
    PROC_SUSPENDED,
    PROC_BLOCKED
} process_state_t;

typedef struct PCB {
    /* --- PHẦN CỐT LÕI (Context Switching) --- */
    uint32_t *stack_ptr;       // Con trỏ stack (quan trọng nhất)
    
    /* --- PHẦN ĐỊNH DANH --- */
    uint32_t pid;              // ID tiến trình
    void (*entry)(void);       // Hàm main của task (để debug hoặc reset task)
    
    /* --- PHẦN TRẠNG THÁI & BLOCKING --- */
    process_state_t state;     // READY, RUNNING, BLOCKED...
    uint32_t wake_up_tick;     // [QUAN TRỌNG] Phải là uint32_t để tránh tràn số
                               // Thời điểm (tick hệ thống) mà task sẽ thức dậy

    /* --- PHẦN LẬP LỊCH (SCHEDULING) --- */
    uint8_t static_priority;     // Độ ưu tiên gốc (Cài đặt ban đầu)
    uint8_t dynamic_priority;  // Độ ưu tiên động (Dùng để lập lịch thực tế)
    
    uint8_t time_slice;        // Số tick còn lại trong lượt chạy hiện tại (Round-robin quota)
    
    /* --- PHẦN THỐNG KÊ (Tùy chọn) --- */
    uint32_t total_cpu_runtime; // Tổng số tick task này đã chiếm dụng từ lúc khởi động
                                // Dùng uint32_t để không bị tràn sớm
    uint8_t id_cpu;           // CPU chạy task này (dành cho hệ thống đa lõi)

    /* --- PHẦN QUẢN LÝ TÀI NGUYÊN (RESOURCE MANAGEMENT) --- */
    int res_held[NUM_RESOURCES]; // Số lượng tài nguyên đang giữ
    int res_max[NUM_RESOURCES]; // Số lượng tài nguyên tối đa có thể yêu cầu

    uint32_t heap_base;    // Địa chỉ cơ sở của heap
    uint32_t heap_size;    // Kích thước của heap
    uint32_t stack_base;   // Địa chỉ cơ sở của stack
    uint32_t stack_size;   // Kích thước của stack
} PCB_t;

extern PCB_t pcb_table[MAX_PROCESSES];


void process_init(void);
void process_create(void (*func)(void), uint32_t pid, uint8_t priority, int *max_res);
void process_admit_jobs(void);
void process_schedule(void);
void process_set_state(uint32_t pid, process_state_t new_state);
const char* process_state_str(process_state_t state);
void os_delay(uint32_t tick);
void process_timer_tick(void);
void add_task_to_ready_queue(PCB_t *p);
PCB_t* get_highest_priority_ready_task(void);
void prvIdleTask(void);
#endif
