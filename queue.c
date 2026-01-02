#include "queue.h"
#include "process.h"

void queue_init(queue_t *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

int queue_is_empty(queue_t *q) {
    return q->count == 0;
}

int queue_is_full(queue_t *q) {
    return q->count == MAX_QUEUE_LEN;
}

void queue_enqueue(queue_t *q, struct PCB *pcb) {
    if (queue_is_full(q)) {
        OS_EXIT_CRITICAL();
        return;
    }
    q->rear = (q->rear + 1) % MAX_QUEUE_LEN;
    q->items[q->rear] = pcb;
    q->count++;
}

struct PCB* queue_dequeue(queue_t *q) {
    OS_ENTER_CRITICAL();
    if (queue_is_empty(q)) {
        OS_EXIT_CRITICAL();
        return 0;
    }
    struct PCB *pcb = q->items[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_LEN;
    q->count--;
    OS_EXIT_CRITICAL();
    return pcb;
}
