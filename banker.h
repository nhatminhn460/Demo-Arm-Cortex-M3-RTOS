#ifndef BANKER_H
#define BANKER_H

typedef enum {
    RES_UART = 0,   // Máy in UART
    RES_I2C,        // Bus cảm biến
    RES_DMA_CH,     // Kênh DMA (giả sử có 2 kênh)
    NUM_RESOURCES   // Tổng số loại (3)
} resource_type_t;

/* Tổng kho tài nguyên của hệ thống (Available) */
/* Ví dụ: 1 UART, 1 I2C, 2 DMA Channels */
extern int system_available[NUM_RESOURCES];

void banker_init(void);
int request_resources(int request[]);
void release_resources(int release[]);

#endif 