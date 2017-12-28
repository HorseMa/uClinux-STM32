MAIN = main.c
OBJS = stm32f10x_gpio.o stm32f10x_rcc.o stm32f10x_fsmc.o lcd.o
PROG = ShowLogo
#CC = arm-uclinuxeabi-gcc
CFLAGS =-Os -march=armv7-m -mthumb -Wl,-elf2flt=-s -Wl,-elf2flt=4096 
INC = -I Libraries/CMSIS/Core/CM3/ -I Libraries/STM32F10x_StdPeriph_Driver/inc/

all:$(MAIN) $(OBJS)
	$(CC) $(CFLAGS) $(MAIN) -o $(PROG) $(OBJS)
stm32f10x_gpio.o: 
	$(CC) $(CFLAGS) -c Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c $(INC)
stm32f10x_rcc.o: 
	$(CC) $(CFLAGS) -c Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c $(INC)
stm32f10x_fsmc.o: 
	$(CC) $(CFLAGS) -c Libraries/STM32F10x_StdPeriph_Driver/src/stm32f10x_fsmc.c $(INC)
lcd.o:
	$(CC) $(CFLAGS) -c lcd.c $(INC)
	
romfs:
	$(ROMFSINST) -e CONFIG_USER_SHOWLOGO /bin/$(PROG)

clean:
	-rm $(PROG) $(PROG).gdb $(OBJS)
	-rm ./Libraries/STM32F10x_StdPeriph_Driver/src/*.o