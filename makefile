TARGET = dualsense_bridge
OBJS = main.o

CC = ppu-gcc
CFLAGS = -O2 -Wall -I$(PS3DEV)/ppu/include
LDFLAGS = -L$(PS3DEV)/ppu/lib -lprx

all:
	$(CC) $(CFLAGS) -c main.c
	$(CC) -o $(TARGET).elf $(OBJS) $(LDFLAGS)
	ppu-strip $(TARGET).elf
	ppu-objcopy -O binary $(TARGET).elf $(TARGET).sprx

clean:
	rm -f *.o *.elf *.sprx
