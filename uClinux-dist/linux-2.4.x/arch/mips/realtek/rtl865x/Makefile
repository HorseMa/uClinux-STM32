#
# Makefile for the Realtek 865x SoC specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(AFLAGS) $< -o $@
.S.o:
	$(CC) $(AFLAGS) -c $< -o $@

O_TARGET	:= rtl865x.o

all: rtl865x.o

#obj-y		:= int-handler.o irq.o setup.o prom.o power.o
obj-y		:= int-handler.o irq.o setup.o prom.o 
export-objs	:= pci.o setup.o
obj-$(CONFIG_PCI)		+= pci.o 

int-handler.o: int-handler.S

clean:
		rm -f *.o

EXTRA_CFLAGS += -I../../../../drivers/net/re865x/rtl865x/.

include $(TOPDIR)/Rules.make
