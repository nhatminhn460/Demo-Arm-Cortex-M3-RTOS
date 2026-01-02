#include "process.h"
#include "uart.h"
#include "queue.h"
#include "memory.h"
#include <stdint.h>
#include "mpu.h"

#define SCB_ICSR (*(volatile uint32_t*)0xE000ED04)
#define PENDSVSET_BIT (1UL << 28)

volatile uint32_t tick_count = 0; // Biến đếm tick hệ thống
PCB_t *current_pcb = NULL;   // PCB hiện tại
PCB_t *next_pcb = NULL;      // PCB tiếp theo sẽ chạy

extern void start_first_task(uint32_t *first_sp); 
uint32_t top_ready_priority_bitmap = 0; // Biến toàn cục để lưu bitmap ưu tiên cao nhất


queue_t job_queue;

queue_t ready_queue[MAX_PRIORITY];
queue_t device_queue;

// static uint32_t stacks[MAX_PROCESSES][STACK_SIZE];
PCB_t pcb_table[MAX_PROCESSES];

static int total_processes = 0; 

const char* process_state_str(process_state_t state) {
    switch (state) {
        case PROC_NEW:        return "NEW";
        case PROC_READY:      return "READY";
        case PROC_RUNNING:    return "RUNNING";
        case PROC_SUSPENDED:  return "SUSPENDED";
        case PROC_BLOCKED:    return "BLOCKED";
        default:              return "UNKNOWN";
    }
}

void process_init(void) {
    uart_print("Process system initialized.\r\n");

    os_mem_init(); // Khởi tạo bộ nhớ động cho PCB và stacks
    for(int i = 0; i < MAX_PROCESSES; i++) {
        queue_init(&ready_queue[i]);
    }
    
    top_ready_priority_bitmap = 0;

    total_processes = 0;
    current_pcb = NULL;
    next_pcb = NULL;

    process_create(prvIdleTask, 0, 0, NULL);
}

void process_create(void (*func)(void), uint32_t pid, uint8_t priority, int *max_res) 
{
    if (pid >= MAX_PROCESSES) return;

    PCB_t *p = &pcb_table[pid];
    uint32_t stack_size_bytes = STACK_SIZE * 4;
    uint32_t *stack_base = (uint32_t*)os_malloc_aligned(stack_size_bytes, mpu_calc_alignment(stack_size_bytes));

    p->stack_base = (uint32_t)stack_base;
    p->stack_size = stack_size_bytes;
    p->heap_base = 0; 
    p->heap_size = 0;

    /* 1. Khởi tạo tài nguyên Banker */
    for (int i = 0; i < NUM_RESOURCES; i++) {
        p->res_held[i] = 0; 
        if (max_res != NULL) {
            p->res_max[i] = max_res[i];
        } else {
            p->res_max[i] = 0;
        }
    }

    /* 2. Cấp phát Stack */
    // Lưu ý: os_malloc trả về byte, ta ép kiểu sang uint32_t*
    //uint32_t *stack_base = (uint32_t*)os_malloc(STACK_SIZE * 4); 
    
    if(stack_base == NULL) {
        uart_print("Error: Heap Full for PID ");
        uart_print_dec(pid);
        uart_print("\r\n");
        return;
    }

    /* --- ĐÂY LÀ DÒNG BẠN BỊ THIẾU --- */
    // Tính toán đỉnh stack (Stack mọc từ địa chỉ cao xuống thấp)
    uint32_t *sp = stack_base + STACK_SIZE; 
    /* -------------------------------- */

    /* 3. Tạo Stack Frame giả lập (Fake Context) */
    *(--sp) = 0x01000000UL;        /* xPSR */
    *(--sp) = (uint32_t)func;      /* PC */
    *(--sp) = 0xFFFFFFFDUL;        /* LR */
    *(--sp) = 0;                   /* R12 */
    *(--sp) = 0;                   /* R3 */
    *(--sp) = 0;                   /* R2 */
    *(--sp) = 0;                   /* R1 */
    *(--sp) = 0;                   /* R0 */

    for (int i = 0; i < 8; ++i) {
        *(--sp) = 0; /* R11 .. R4 */
    }

    /* 4. Lưu thông tin vào PCB */
    p->stack_ptr = sp;         // Lưu đỉnh stack mới
    p->pid = pid;
    p->entry = func;
    p->state = PROC_NEW; 
    p->dynamic_priority = priority;
    p->static_priority = priority;
    p->time_slice = 5;
    p->total_cpu_runtime = 0;
    p->wake_up_tick = 0;

    /* 5. Đưa vào hàng đợi */
    OS_ENTER_CRITICAL();
    add_task_to_ready_queue(p); 
    OS_EXIT_CRITICAL();

    uart_print("Created process ");
    uart_print_dec(pid);
    uart_print(" -> state: ");
    uart_print(process_state_str(p->state));
    uart_print("\r\n");

    total_processes++;
    
    // Nếu task mới có quyền cao hơn task hiện tại -> Preempt (chiếm quyền) ngay
    if(current_pcb && p->dynamic_priority > current_pcb->dynamic_priority) {
        SCB_ICSR |= PENDSVSET_BIT;
    }
}


void process_schedule(void) {
    OS_ENTER_CRITICAL();

    // 1. Kiểm tra nếu không có process READY
    if (top_ready_priority_bitmap == 0) {
    OS_EXIT_CRITICAL();
    return;}

    // 2. Lấy process tiếp theo từ hàng đợi READY
    PCB_t *pnext = get_highest_priority_ready_task();
    if (!pnext) return;

    // 3. Xử lý task hiện tại
    if (current_pcb != NULL) {
        // SỬA 5: Logic quan trọng cho Blocking!
        // Chỉ enqueue lại nếu nó vẫn đang RUNNING (tức là hết giờ time-slice).
        // Nếu nó gọi os_delay, state đã là BLOCKED -> KHÔNG enqueue lại.
        if (current_pcb->state == PROC_RUNNING) {
            current_pcb->state = PROC_READY;
            // queue_enqueue(&ready_queue, current_pcb);
            add_task_to_ready_queue(current_pcb);
        }
    }

    pnext->state = PROC_RUNNING;
    mpu_config_for_task(pnext);
    OS_EXIT_CRITICAL();

    uart_print("Switching to process ");
    uart_print_dec(pnext->pid);
    uart_print(" (");
    uart_print(process_state_str(pnext->state));
    uart_print(")\r\n");

    /* Nếu chưa có process nào chạy*/
    if (current_pcb == NULL) {
        current_pcb = pnext;
        /*set PSP và 'exception return' vào task đầu */
        start_first_task(current_pcb->stack_ptr);
    } else {
        next_pcb = pnext;
        SCB_ICSR |= PENDSVSET_BIT;
    }
}

/* */
void os_delay(uint32_t ticks) {
    // 1. Tính thời điểm báo thức
    // Nếu ticks = 100, hiện tại là 500 -> wake_up_tick = 600
    current_pcb->wake_up_tick = tick_count + ticks;

    // 2. Chuyển trạng thái sang BLOCKED
    current_pcb->state = PROC_BLOCKED;

    // gọi scheduler để tìm task mới 
    process_schedule();
    // 3. Nhường CPU ngay lập tức!
    // Không chờ hết time-slice, ta kích hoạt PendSV để đổi task ngay
    // SCB_ICSR |= PENDSVSET_BIT; (Bạn đã define macro này rồi)
    //*(volatile uint32_t*)0xE000ED04 |= (1UL << 28); 
}

void process_timer_tick(void) {
    tick_count++; // Tăng giờ hệ thống
    int need_schedule = 0;
    /* Quét mảng để tìm Task ngủ dậy */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB_t *p = &pcb_table[i];
        
        // Chỉ kiểm tra task nào đang BLOCKED và có PID hợp lệ (đã tạo)
        if (p->state == PROC_BLOCKED) {
            if (p->wake_up_tick <= tick_count) {
                // Dậy thôi!
                p->state = PROC_READY;
                p->wake_up_tick = 0; // Reset
                
                // Đưa lại vào hàng đợi READY
                // queue_enqueue(&ready_queue, p);
                add_task_to_ready_queue(p);
                need_schedule = 1;
                // (Optional) Debug log
                // uart_print("Task woken up: "); uart_print_dec(p->pid); uart_print("\r\n");
            }
        }
        if(need_schedule) {
            SCB_ICSR |= PENDSVSET_BIT;
        }
    }
}

void add_task_to_ready_queue(PCB_t *p) {
    // lấy độ ưu tiên
    uint8_t prio = p->dynamic_priority;

    // bảo vệ giưới hạn mảng
    if(prio >= MAX_PRIORITY) {
        prio = MAX_PRIORITY - 1;
    }

    // đưa task vào hàng đợi tương ứng
    queue_enqueue(&ready_queue[prio], p);

    // cập nhật bitmap
    top_ready_priority_bitmap |= (1UL << prio);
}

PCB_t* get_highest_priority_ready_task() {
    // tìm độ ưu tiên cao nhất từ bitmap
    for(int prio = MAX_PRIORITY - 1; prio >= 0; prio--) {
        if(top_ready_priority_bitmap & (1UL << prio)) {
            // lấy task từ hàng đợi tương ứng
            PCB_t *p = queue_dequeue(&ready_queue[prio]);
            if(queue_is_empty(&ready_queue[prio])) {
                // nếu hàng đợi trống, xóa bit khỏi bitmap
                top_ready_priority_bitmap &= ~(1UL << prio);
            }
            uart_print("Selected process use priority ");
            return p;
        }
    }
    return NULL; // không có task READY
}

void prvIdleTask(void){
    while(1){
        __asm("wfi"); // Chờ ngắt để tiết kiệm điện
    }
}

