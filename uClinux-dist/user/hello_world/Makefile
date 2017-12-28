MAIN = main.c
#CC = arm-uclinuxeabi-gcc
PROG = hello_world
CFLAGS =-Os -march=armv7-m -mthumb -Wl,-elf2flt=-s -Wl,-elf2flt=4096

all:$(HELLO)
	$(CC) $(CFLAGS) $(MAIN) -o $(PROG)
romfs:
	$(ROMFSINST) -e CONFIG_USER_HELLO /bin/$(PROG)
clean:
	-rm $(PROG) $(PROG).gdb 