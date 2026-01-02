#ifndef TASK_H
#define TASK_H

#include "process.h" 
#include "sync.h"
#include <stdint.h>

/* Biến toàn cục "Giả lập phần cứng" (Shared Resource) */
/* LƯU Ý: Trong OS thực tế, truy cập biến này cần Mutex để tránh tranh chấp! */
extern volatile int current_temperature; 
extern volatile int system_uptime;

void task_sensor_update(void);
void task_display(void);
void task_alarm(void);
void task_logger(void);
void task_shell(void);
void task_deadlock_1(void);
void task_deadlock_2(void);
void task_banker1(void);
void task_banker2(void);
void test_mpu_fault(void);

#endif 