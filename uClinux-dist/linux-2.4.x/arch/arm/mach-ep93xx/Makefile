#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true
EXTRA_LDFLAGS		+= --no-warn-mismatch

O_TARGET		:= ep93xx.o 

# Object file lists.

obj-y			:= arch.o irq.o mm.o time.o ssp.o dma_ep93xx.o pcmcia_io.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= pcipool.o

obj-$(CONFIG_EP93XX_CRUNCH) += crunch.o
obj-$(CONFIG_KGDB_SERIAL) += kgdb-serial.o

include $(TOPDIR)/Rules.make
