#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include "sync.h"

#define MAX_MESSAGE_COUNT 10

typedef struct{
    int32_t buffer[MAX_MESSAGE_COUNT];
    int head;
    int tail;

    os_mutex_t mutex_lock; // Khóa bảo vệ truy cập vào hàng đợi
    os_sem_t sem_data; // Số lượng dữ liệu hiện có
    os_sem_t sem_space; // Số lượng chỗ trống còn lại

} os_msg_queue_t;

void msg_queue_init(os_msg_queue_t *q); // Khởi tạo hàng đợi tin nhắn
void msg_queue_send(os_msg_queue_t *q, int32_t data); // Gửi tin nhắn vào hàng đợi
int32_t msg_queue_receive(os_msg_queue_t *q); // Nhận tin nhắn từ hàng đợi

#endif