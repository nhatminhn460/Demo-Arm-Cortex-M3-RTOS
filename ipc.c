#include "ipc.h"

void msg_queue_init(os_msg_queue_t *q){
    q->head = 0;
    q->tail = 0;

    mutex_init(&q->mutex_lock); // Khởi tạo mutex
    sem_init(&q->sem_data, 0); // Ban đầu không có dữ liệu
    sem_init(&q->sem_space, MAX_MESSAGE_COUNT); // Ban đầu có chỗ trống đầy đủ
}

void msg_queue_send(os_msg_queue_t *q, int32_t data){
    sem_wait(&q->sem_space); // Chờ có chỗ trống

    mutex_lock(&q->mutex_lock); // Khóa truy cập

    q->buffer[q->head] = data;
    q->head = (q->head + 1) % MAX_MESSAGE_COUNT;

    mutex_unlock(&q->mutex_lock); // Mở khóa

    sem_signal(&q->sem_data); // Tăng số lượng dữ liệu
}

int32_t msg_queue_receive(os_msg_queue_t *q){
    int32_t data;
    sem_wait(&q->sem_data); // Chờ có dữ liệu

    mutex_lock(&q->mutex_lock); // Khóa truy cập

    data = q->buffer[q->tail];
    q->tail = (q->tail + 1) % MAX_MESSAGE_COUNT;

    mutex_unlock(&q->mutex_lock); // Mở khóa

    sem_signal(&q->sem_space); // Tăng số chỗ trống

    return data;
}