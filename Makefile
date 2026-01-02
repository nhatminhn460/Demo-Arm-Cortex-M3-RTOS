TARGET = kernel
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

# Thêm -g để có thể debug, -Wall để hiện cảnh báo code
CFLAGS = -mcpu=cortex-m3 -mthumb -O2 -ffreestanding -nostdlib -g -Wall
LDFLAGS = -T linker.ld -nostdlib

# QUAN TRỌNG: Đã thêm context_switch.s vào danh sách biên dịch
SRC = main.c startup.s context_switch.s uart.c systick.c process.c queue.c task.c sync.c ipc.c  memory.c banker.c mpu.c

all: $(TARGET).bin

$(TARGET).elf: $(SRC) linker.ld
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

run:
	qemu-system-arm -M lm3s6965evb -kernel $(TARGET).bin -serial mon:stdio -nographic

clean:
	rm -f $(TARGET).elf $(TARGET).bin