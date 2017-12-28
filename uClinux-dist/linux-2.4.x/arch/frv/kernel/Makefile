#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

all: head.o entry-table.o kernel.o

export-objs := frv_ksyms.o semaphore.o dma.o

O_TARGET := kernel.o

obj-y := entry.o break.o switch_to.o kernel_thread.o \
	 process.o traps.o ptrace.o signal.o nmi.o dma.o \
	 sys_frv.o time.o semaphore.o setup.o frv_ksyms.o \
	 debug-stub.o irq.o irq-routing.o sleep.o uaccess.o

obj-$(CONFIG_MMU_LINUX)		+= head-mmu-fr451.o
obj-$(CONFIG_UCLINUX)		+= head-uc-fr401.o head-uc-fr451.o head-uc-fr555.o

obj-$(CONFIG_GDBSTUB)		+= gdb-stub.o gdb-io.o

obj-$(CONFIG_MB93091_VDK)	+= irq-mb93091.o
obj-$(CONFIG_MB93093_PDK)	+= irq-mb93093.o
obj-$(CONFIG_FUJITSU_MB93493)	+= irq-mb93493.o
obj-$(CONFIG_PM)		+= pm.o cmode.o
obj-$(CONFIG_SYSCTL)		+= sysctl.o

ifeq ($(CONFIG_MB93093_PDK),y)
obj-$(CONFIG_PM)		+= pm-mb93093.o
endif

include $(TOPDIR)/Rules.make

