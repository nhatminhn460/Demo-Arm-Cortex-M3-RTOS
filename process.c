#include "process.h"
#include "uart.h"
#include "queue.h"
#include "memory.h"
#include <stdint.h>
#include "mpu.h"

#define SCB_ICSR (*(volatile uint32_t*)0xE000ED04)
#define PENDSVSET_BIT (1UL << 28)

volatile uint32_t tick_count = 0;
PCB_t *current_pcb = NULL;
PCB_t *next_pcb = NULL;

extern void start_first_task(PCB_t *first_task);  
uint32_t top_ready_priority_bitmap = 0;

queue_t job_queue;
queue_t ready_queue[MAX_PRIORITY];
queue_t device_queue;

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

    os_mem_init();
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
    if (stack_size_bytes < 256) stack_size_bytes = 256;
    
    stack_size_bytes--;
    stack_size_bytes |= stack_size_bytes >> 1;
    stack_size_bytes |= stack_size_bytes >> 2;
    stack_size_bytes |= stack_size_bytes >> 4;
    stack_size_bytes |= stack_size_bytes >> 8;
    stack_size_bytes |= stack_size_bytes >> 16;
    stack_size_bytes++;
    
    uint32_t required_alignment = mpu_calc_alignment(stack_size_bytes);
    uint32_t *stack_base = (uint32_t*)os_malloc_aligned(stack_size_bytes, required_alignment);
    
    if (stack_base == NULL) {
        uart_print("ERROR: Heap full for PID ");
        uart_print_dec(pid);
        uart_print("\r\n");
        return;
    }
    
    if ((uint32_t)stack_base % required_alignment != 0) {
        uart_print("ERROR: Stack not aligned! Base=0x");
        uart_print_hex32((uint32_t)stack_base);
        uart_print(" Required=");
        uart_print_dec(required_alignment);
        uart_print("\r\n");
        os_free(stack_base);
        return;
    }
    
    p->stack_base = (uint32_t)stack_base;
    p->stack_size = stack_size_bytes;
    p->heap_base = 0; 
    p->heap_size = 0;

    /* Initialize Banker's algorithm resources */
    for (int i = 0; i < NUM_RESOURCES; i++) {
        p->res_held[i] = 0; 
        p->res_max[i] = (max_res != NULL) ? max_res[i] : 0;
    }

    /* Calculate stack pointer (grows downward) */
    uint32_t *sp = stack_base + (stack_size_bytes / 4);

    /* Create fake stack frame */
    *(--sp) = 0x01000000UL;        /* xPSR */
    *(--sp) = (uint32_t)func;      /* PC */
    *(--sp) = 0xFFFFFFFDUL;        /* LR */
    *(--sp) = 0;                   /* R12 */
    *(--sp) = 0;                   /* R3 */
    *(--sp) = 0;                   /* R2 */
    *(--sp) = 0;                   /* R1 */
    *(--sp) = 0;                   /* R0 */

    for (int i = 0; i < 8; ++i) {
        *(--sp) = 0;               /* R11-R4 */
    }

    /* Initialize PCB */
    p->stack_ptr = sp;
    p->pid = pid;
    p->entry = func;
    p->state = PROC_NEW;
    p->dynamic_priority = priority;
    p->static_priority = priority;
    p->time_slice = 5;
    p->total_cpu_runtime = 0;
    p->wake_up_tick = 0;

    /* Add to ready queue */
    OS_ENTER_CRITICAL();
    add_task_to_ready_queue(p);
    OS_EXIT_CRITICAL();

    uart_print("Created process ");
    uart_print_dec(pid);
    uart_print(" -> state: ");
    uart_print(process_state_str(p->state));
    uart_print("\r\n");

    total_processes++;
    
    /* Preempt if higher priority */
    if (current_pcb && p->dynamic_priority > current_pcb->dynamic_priority) {
        SCB_ICSR |= PENDSVSET_BIT;
    }
}

void process_schedule(void) {
    OS_ENTER_CRITICAL();

    if (top_ready_priority_bitmap == 0) {
        OS_EXIT_CRITICAL();  
        return;
    }

    PCB_t *pnext = get_highest_priority_ready_task();
    if (!pnext) {
        OS_EXIT_CRITICAL(); 
        return;
    }

    if (current_pcb != NULL && current_pcb->state == PROC_RUNNING) {
        current_pcb->state = PROC_READY;
        add_task_to_ready_queue(current_pcb);
    }

    pnext->state = PROC_RUNNING;
    OS_EXIT_CRITICAL();  

    uart_print("Switching to process ");
    uart_print_dec(pnext->pid);
    uart_print(" (");
    uart_print(process_state_str(pnext->state));
    uart_print(")\r\n");

    /* MPU config happens in start_first_task() or PendSV_Handler */
    if (current_pcb == NULL) {
        current_pcb = pnext;
        start_first_task(current_pcb);  
    } else {
        next_pcb = pnext;
        SCB_ICSR |= PENDSVSET_BIT;
    }
}

void os_delay(uint32_t ticks) {
    current_pcb->wake_up_tick = tick_count + ticks;
    current_pcb->state = PROC_BLOCKED;
    process_schedule();
}

void process_timer_tick(void) {
    tick_count++;
    int need_schedule = 0;
    
    OS_ENTER_CRITICAL();  
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB_t *p = &pcb_table[i];
        
        if (p->state == PROC_BLOCKED && p->wake_up_tick <= tick_count) {
            p->state = PROC_READY;
            p->wake_up_tick = 0;
            add_task_to_ready_queue(p);
            need_schedule = 1;
        }
    }
    
    OS_EXIT_CRITICAL(); 
    
    if (need_schedule) {
        SCB_ICSR |= PENDSVSET_BIT;
    }
}

void add_task_to_ready_queue(PCB_t *p) {
    uint8_t prio = p->dynamic_priority;
    
    if (prio >= MAX_PRIORITY) {
        prio = MAX_PRIORITY - 1;
    }
    
    queue_enqueue(&ready_queue[prio], p);
    top_ready_priority_bitmap |= (1UL << prio);
}

PCB_t* get_highest_priority_ready_task() {
    for (int prio = MAX_PRIORITY - 1; prio >= 0; prio--) {
        if (top_ready_priority_bitmap & (1UL << prio)) {
            PCB_t *p = queue_dequeue(&ready_queue[prio]);
            if (queue_is_empty(&ready_queue[prio])) {
                top_ready_priority_bitmap &= ~(1UL << prio);
            }
            return p;
        }
    }
    return NULL;
}

void prvIdleTask(void) {
    while (1) {
        __asm("wfi");
    }
}