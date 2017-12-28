MAIN = main.c
PROG = Toggle_gpio
#CC = arm-uclinuxeabi-gcc
CFLAGS =-Os -march=armv7-m -mthumb -Wl,-elf2flt=-s -Wl,-elf2flt=8192

all:$(MAIN)
	$(CC) $(CFLAGS) $(MAIN) -o $(PROG)
romfs:
	$(ROMFSINST) -e CONFIG_USER_TOGGLE_GPIO /bin/$(PROG)
clean:
	-rm $(PROG) $(PROG).gdb