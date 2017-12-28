MAIN = main.c
PROG = Led_Show
#CC = arm-uclinuxeabi-gcc
CFLAGS =-Os -march=armv7-m -mthumb -Wl,-elf2flt=-s -Wl,-elf2flt=4096

all:$(MAIN)
	$(CC) $(CFLAGS) $(MAIN) -o $(PROG)
romfs:
	$(ROMFSINST) -e CONFIG_USER_LED_SHOW /bin/$(PROG)
clean:
	-rm $(PROG) $(PROG).gdb