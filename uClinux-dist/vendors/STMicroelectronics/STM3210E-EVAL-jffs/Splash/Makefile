all:
	$(CC) -O3 -march=armv7-m -mthumb -Wl,-elf2flt=-s -Wl,-elf2flt=1024 main.c -o Splash
romfs:
	$(ROMFSINST) /bin/Splash
clean:
	-rm Splash Splash.gdb