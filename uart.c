#include "uart.h"
#include "sync.h"

// địa chỉ vật lý của các thanh ghi 
#define UART0_BASE  0x4000C000 // địa chỉ vật lý của uart0
#define UART0_DR (*(volatile unsigned int*)(UART0_BASE + 0x00)) // ghi vào thanh ghi này để gửi đi, đọc từ đây để nhận về dữ liệu
#define UART0_FR (*(volatile uint32_t*)(UART0_BASE + 0x018)) // bảng thông báo chứa các cờ trạng thái(FIFO đang đầy / trống, uart đang bận hay không)
#define UART0_IM (*(volatile uint32_t*)(UART0_BASE + 0x038)) // công tắc để cho phép chặn các ngắt UART , nếu bit = 1 -> cho phép ngắt
#define UART0_ICR (*(volatile uint32_t*)(UART0_BASE + 0x044)) // ghi 1 để xóa cờ ngắt

#define NVIC_EN0 (*(volatile uint32_t*)0xE000E100) // bật ngắt cho ngoại vi, thanh ghi enable interrupt của NVIC (arm cortex - M)

#define UART_RXFE      (1 << 4) // FIFO Empty -> ko có dữ liệu đọc
#define UART_TXFF      (1 << 5) // FIFO full -> ko thể ghi thêm dữ liệu
#define UART_RXIM      (1 << 4)  // cho phép ngắt khi có dữ liệu cho RX

#define RX_BUFFER_SIZE 128 // buffer vòng
static char rx_bufferr[RX_BUFFER_SIZE]; 
static int rx_head = 0; //  vị trí ghi
static int rx_tail = 0; // vị trí đọc

os_sem_t uart_rx_semaphore; 

void uart_init(void) {
    sem_init(&uart_rx_semaphore, 0); // giá trị ban đầu là 0 , chưa có dữ liệu uart
    UART0_IM |= UART_RXIM;
    NVIC_EN0 |= (1 << 5); 
}

void uart_putc(char c) {
    while (UART0_FR & UART_TXFF); // chờ đến khi có chỗ trống
    UART0_DR = c;
}

void uart_print(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

void uart_print_dec(uint32_t val) {
    char buf[10];
    int i = 0;

    if (val == 0) {
        uart_putc('0');
        return;
    }

    while (val > 0 && i < sizeof(buf)) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

void UART0_Handler(void) {
    UART0_ICR |= UART_RXIM;
    while((UART0_FR & UART_RXFE) == 0){
        char c = (char)(UART0_DR & 0xFF);
        int next_head = (rx_head + 1) % RX_BUFFER_SIZE;
        if (next_head != rx_tail) {
            rx_bufferr[rx_head] = c;
            rx_head = next_head;
            sem_signal(&uart_rx_semaphore);
        }
    }
}


char uart_getc(void) {
    sem_wait(&uart_rx_semaphore);
    OS_ENTER_CRITICAL();

    char c = rx_bufferr[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;

    OS_EXIT_CRITICAL();

    return c;
}

// Hàm phụ trợ để chuyển số 0-15 thành ký tự '0'-'F'
static char nibble_to_hex(uint8_t nibble) {
    if (nibble < 10) return '0' + nibble;
    return 'A' + (nibble - 10);
}

void uart_print_hex(uint8_t n) {
    char str[3];
    str[0] = nibble_to_hex((n >> 4) & 0x0F); // 4 bit cao
    str[1] = nibble_to_hex(n & 0x0F);        // 4 bit thấp
    str[2] = '\0'; // Kết thúc chuỗi
    uart_print(str);
}

void uart_print_hex32(uint32_t n) {
    char str[11];
    str[0] = '0';
    str[1] = 'x';
    
    // In từ byte cao nhất (MSB) xuống thấp nhất
    for (int i = 0; i < 8; i++) {
        // Dịch để lấy từng cụm 4 bit (nibble), bắt đầu từ bit 28
        uint8_t nibble = (n >> (28 - (i * 4))) & 0x0F;
        str[2 + i] = nibble_to_hex(nibble);
    }
    str[10] = '\0';
    uart_print(str);
}
