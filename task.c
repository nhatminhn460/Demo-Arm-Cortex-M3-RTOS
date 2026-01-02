#include "task.h"
#include "uart.h"
#include "process.h" 
#include "sync.h"    
#include "ipc.h"
#include "uart.h"
#include "banker.h"
#include <stdint.h>

/* Biến toàn cục */
volatile int current_temperature = 25; 
volatile int system_uptime = 0;
extern os_mutex_t app_mutex;
extern os_msg_queue_t temp_queue;
extern os_mutex_t mutex_A;
extern os_mutex_t mutex_B;

/* TASK 1: SENSOR */
void task_sensor_update(void) {
    int local_temp = 25; 
    int direction = 1; 
    while (1) {
        os_delay(10); 
        
        if(direction == 1){
            local_temp += 5;
            if(local_temp >= 55) direction = -1;
        } else {
            local_temp -= 5;
            if(local_temp <= 20) direction = 1;
        }

        msg_queue_send(&temp_queue, local_temp);
    }
}

/* TASK 2: DISPLAY */
void task_display(void) {
    int received_temp; 

    while (1) {
        // 1. Nhận dữ liệu từ hàng đợi IPC
        received_temp = msg_queue_receive(&temp_queue);
        
        // 2. Cập nhật biến toàn cục (để Task Alarm dùng nếu cần)
        current_temperature = received_temp;

        // 3. In ra màn hình giá trị VỪA NHẬN ĐƯỢC
        mutex_lock(&app_mutex);
            uart_print("----------------------\r\n");
            uart_print("| Temp: "); 
            
            // SỬA QUAN TRỌNG: In received_temp (số mới) thay vì current_temperature (số cũ)
            uart_print_dec(received_temp); 
            
            uart_print(" C         |\r\n");
            uart_print("----------------------\r\n");
        mutex_unlock(&app_mutex);
    }
}

/* TASK 3: ALARM */
void task_alarm(void) {
    int alarm_active = 0; 

    while (1) {
        os_delay(5);
        
        mutex_lock(&app_mutex);
        if (current_temperature > 40) {
            if (alarm_active == 0) {
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                uart_print("\r\n!!! [ALARM] WARNING: OVERHEAT !!!\r\n");
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                alarm_active = 1; 
            }
        } 
        else {
            if (alarm_active == 1) {
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                uart_print("\r\n[ALARM] Temperature Normal.\r\n");                
                uart_print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
                alarm_active = 0; 
            }
        }
        mutex_unlock(&app_mutex);
    }
}

/* ------------------------------------------------
   TASK 4: LOGGER (Dùng để test Round-Robin)
   Mục tiêu: Chạy song song với Sensor cùng Priority
   ------------------------------------------------ */
void task_logger(void) {
    int counter = 0;
    
    while (1) {
        // Delay giống hệt Sensor để 2 ông này thức dậy cùng lúc
        // và tranh giành CPU
        os_delay(10); 

        mutex_lock(&app_mutex);
        uart_print("    >>> [LOGGER] Checking system... Count: ");
        uart_print_dec(counter++);
        uart_print("\r\n");
        mutex_unlock(&app_mutex);
    }
}

// Hàm so sánh chuỗi đơn giản
int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* ------------------------------------------------
   TASK 4: SHELL (Giao diện dòng lệnh)
   ------------------------------------------------ */
void task_shell(void) {
    char cmd_buffer[32];
    int cmd_index = 0;

    mutex_lock(&app_mutex);
    uart_print("\r\n[SHELL] Ready. Type 'help' to start.\r\n");
    uart_print("MyOS> ");
    mutex_unlock(&app_mutex);

    while (1) {
        // Hàm này sẽ ngủ cho đến khi bạn gõ phím
        char c = uart_getc();

        // Echo lại ký tự ra màn hình (để bạn thấy mình gõ gì)
        mutex_lock(&app_mutex);
        uart_putc(c);
        mutex_unlock(&app_mutex);

        // Xử lý phím Enter (\r)
        if (c == '\r') {
            mutex_lock(&app_mutex);
            uart_print("\n"); // Xuống dòng
            cmd_buffer[cmd_index] = '\0'; // Kết thúc chuỗi

            /* --- XỬ LÝ LỆNH --- */
            if (my_strcmp(cmd_buffer, "help") == 0) {
                uart_print("Available commands:\r\n");
                uart_print("  help  : Show this help\r\n");
                uart_print("  temp  : Show current temperature\r\n");
                uart_print("  reboot: Restart system\r\n");
            } 
            else if (my_strcmp(cmd_buffer, "temp") == 0) {
                uart_print("Current Temp: ");
                uart_print_dec(current_temperature);
                uart_print(" C\r\n");
            }
            else if (my_strcmp(cmd_buffer, "reboot") == 0) {
                uart_print("Rebooting...\r\n");
                // Reset bằng cách ghi vào AIRCR của SCB
                *(volatile uint32_t*)0xE000ED0C = 0x05FA0004;
            }
            else if (cmd_index > 0) {
                uart_print("Unknown command: ");
                uart_print(cmd_buffer);
                uart_print("\r\n");
            }

            uart_print("MyOS> ");
            mutex_unlock(&app_mutex);
            
            // Reset buffer
            cmd_index = 0;
        } 
        else {
            // Lưu ký tự vào buffer
            if (cmd_index < 31) {
                cmd_buffer[cmd_index++] = c;
            }
        }
    }
}

void task_deadlock_1(){
    while(1){
        // lấy khóa A
        mutex_lock(&mutex_A);
        uart_print("Task 1: Got A. Waitting for B ... \r\n");

        os_delay(10); // ngủ để các task khác chạy
        // cố lấy khóa B
        mutex_lock(&mutex_B);
        uart_print("Task 1: Got both!\r\n");
        mutex_unlock(&mutex_B);
        mutex_unlock(&mutex_A);
    }
}

void task_deadlock_2(){
    while(1){
        // lấy khóa B
        mutex_lock(&mutex_B);
        uart_print("Task 2: Got B. Waitting for A ... \r\n");

        os_delay(10); // ngủ để các task khác chạy
        // cố lấy khóa A
        mutex_lock(&mutex_A);
        uart_print("Task 2: Got both!\r\n");
        mutex_unlock(&mutex_A);
        mutex_unlock(&mutex_B);
    }
}

void task_banker1(void){
    int req[] = {0, 0, 1}; // xin 0 uart, 0 i2c, 1 DMA
    while(1){
        uart_print(" T1 : Asking for 1 DMA ...\r\n");

        if(request_resources(req)){
            uart_print("T1 : granted 1 DMA ! Holding it .... \r\n");

            // T1 giữ tài nguyên và làm việc rất lâu
            // -> tài nguyên đang bị giam lỏng

            os_delay(100);

            uart_print("T1 : Releasing DMA. \r\n");
            release_resources(req);

        }
        else{
            uart_print("T1 releasing DMA. \r\n");

        }

        os_delay(20);
    }
}

/* TASK TEST 2: Người đến muộn gây nguy hiểm */
void task_banker2(void) {
    int req[] = {0, 0, 1}; // Cũng xin 1 DMA
    
    while(1) {
        // Đợi T1 chạy trước một chút để tạo tình huống tranh chấp
        os_delay(10); 
        
        uart_print("T2: Asking for 1 DMA...\r\n");
        
        /* Theo kịch bản: T1 đã giữ 1 DMA. Hệ thống còn 1.
           Nhưng T2 cần Max là 2.
           Nếu cấp nốt 1 DMA còn lại cho T2 -> Hệ thống còn 0.
           -> UNSAFE STATE (Cả T1 và T2 đều có thể đòi thêm 1 nữa và kẹt cứng).
           -> Banker sẽ TỪ CHỐI T2.
        */
        if (request_resources(req)) {
            uart_print("T2: GRANTED! (Strange?)\r\n");
            release_resources(req);
        } else {
            uart_print("T2: DENIED by Banker (Unsafe State)!\r\n");
        }
        
        os_delay(100);
    }
}

void test_mpu_fault(void)
{
    volatile uint32_t *p = (uint32_t *)0x00000000;
    *p = 123;   // ghi flash → MPU fault
}