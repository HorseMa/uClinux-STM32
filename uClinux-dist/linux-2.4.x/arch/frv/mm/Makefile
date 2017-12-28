#
# Makefile for the linux m68k-specific parts of the memory manager.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := mm.o
obj-y	 := init.o kmap.o

obj-$(CONFIG_MMU_LINUX) += \
	pgalloc.o fault.o extable.o cache-page.o tlb-flush.o tlb-miss.o \
	mmu-context.o dma-alloc.o unaligned.o elf-fdpic.o

include $(TOPDIR)/Rules.make
