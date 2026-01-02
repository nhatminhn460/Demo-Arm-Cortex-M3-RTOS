#include "banker.h"
#include "process.h"
#include "uart.h"
#include "sync.h" 

/* 1. Kho tài nguyên thực tế của hệ thống */
int system_available[NUM_RESOURCES];

/* 2. Khởi tạo */
void banker_init(void){
    system_available[RES_UART] = 1;
    system_available[RES_I2C] = 1;
    system_available[RES_DMA_CH] = 2;
    uart_print("[BANKER] System resources initialized. \r\n");
}

/* 3. Thuật toán kiểm tra an toàn (Safety Check) */
int is_safe_state(void){
    int work[NUM_RESOURCES];   // SỬA: word -> work
    int finish[MAX_PROCESSES];

    /* A. Khởi tạo Work = Available */
    for(int i = 0; i < NUM_RESOURCES; i++){
        work[i] = system_available[i];
    }

    /* B. Khởi tạo Finish */
    for(int i = 0; i < MAX_PROCESSES; i++){
        PCB_t *p  = &pcb_table[i];
        
        // SỬA: Dùng == thay vì =
        if(p->state == PROC_NEW || p->pid == 0){
            finish[i] = 1; // Coi như xong
        }
        else{
            finish[i] = 0;
        }
    }
    
    /* C. Vòng lặp tìm chuỗi an toàn */
    while(1){
        int found_task = 0;

        // SỬA: Dấu phẩy -> Dấu chấm phẩy ; và dấu bé hơn <
        for(int i = 0; i < MAX_PROCESSES; i++){ 
            PCB_t *p = &pcb_table[i];

            if(finish[i] == 0){
                int can_allocate = 1;

                // Kiểm tra Need <= Work ?
                for (int r = 0; r < NUM_RESOURCES; r++){
                    // SỬA: res_hold -> res_held
                    int need = p->res_max[r] - p->res_held[r];
                    if(need > work[r]){
                        can_allocate = 0; // Không đủ
                        break;
                    }
                }
                
                // Nếu đủ (Work >= Need)
                if(can_allocate){
                    // Giả sử task xong -> Trả tài nguyên
                    for(int r = 0; r < NUM_RESOURCES; r++){
                        // SỬA: res_hold -> res_held
                        work[r] += p->res_held[r];
                    }
                    finish[i] = 1;
                    found_task = 1;
                }
            }
        }
        
        if(found_task == 0) break; // Không tìm được task nào nữa
    }

    /* D. Kiểm tra kết quả */
    // SỬA: Dấu phẩy -> Dấu chấm phẩy
    for(int i = 0; i < MAX_PROCESSES; i++){
        if(finish[i] == 0) return 0; // UNSAFE
    }
    return 1; // SAFE
}

/* 4. HÀM XIN TÀI NGUYÊN */
int request_resources(int request[]) {
    PCB_t *p = current_pcb;
    if (p == NULL) return 0;

    OS_ENTER_CRITICAL(); 

    /* BƯỚC A: Kiểm tra hợp lệ */
    for (int i = 0; i < NUM_RESOURCES; i++) {
        // SỬA: res_hold -> res_held
        int need = p->res_max[i] - p->res_held[i];
        if (request[i] > need) {
            uart_print("Banker: Error! Request > Need.\r\n");
            OS_EXIT_CRITICAL();
            return 0;
        }
        if (request[i] > system_available[i]) {
            OS_EXIT_CRITICAL();
            return 0; 
        }
    }

    /* BƯỚC B: Giả lập cấp phát */
    for (int i = 0; i < NUM_RESOURCES; i++) {
        system_available[i] -= request[i];
        p->res_held[i]      += request[i]; // SỬA: res_held
    }

    /* BƯỚC C: Kiểm tra an toàn */
    if (is_safe_state()) {
        OS_EXIT_CRITICAL();
        return 1; // OK
    } else {
        /* Rollback */
        for (int i = 0; i < NUM_RESOURCES; i++) {
            system_available[i] += request[i];
            p->res_held[i]      -= request[i]; // SỬA: res_held
        }
        OS_EXIT_CRITICAL();
        return 0; // Denied
    }
}

/* 5. HÀM TRẢ TÀI NGUYÊN */
void release_resources(int release[]) {
    PCB_t *p = current_pcb;
    OS_ENTER_CRITICAL();
    
    for (int i = 0; i < NUM_RESOURCES; i++) {
        p->res_held[i]      -= release[i]; // SỬA: res_held
        system_available[i] += release[i];
    }
    
    OS_EXIT_CRITICAL();
}