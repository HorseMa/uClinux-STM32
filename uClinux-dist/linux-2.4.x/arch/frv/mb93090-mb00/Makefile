#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := motherboard.o

obj-$(CONFIG_PCI) := pci-frv.o pci-dma.o pci-irq.o pci-vdk.o

export-objs :=

include $(TOPDIR)/Rules.make

