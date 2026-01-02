#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stddef.h>

struct PCB;
#define MAX_QUEUE_LEN 10 

typedef struct {
    struct PCB* items[MAX_QUEUE_LEN];
    int front;
    int rear;
    int count;
} queue_t;

// Hàm hàng đợi cơ bản
void queue_init(queue_t *q);
int queue_is_empty(queue_t *q);
int queue_is_full(queue_t *q);
void queue_enqueue(queue_t *q, struct PCB *pcb);
struct PCB* queue_dequeue(queue_t *q);

#endif
