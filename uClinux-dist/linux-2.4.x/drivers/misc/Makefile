#
# Makefile for misc devices that really don't fit anywhere else.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

O_TARGET := misc.o


mod-subdirs	+=	fslfifo
subdir-$(CONFIG_MICROBLAZE_FSLFIFO) += fslfifo
obj-$(CONFIG_MICROBLAZE_FSLFIFO) += fslfifo/fslfifo.o

mod-subdirs	+=	xilinx_hwicap
subdir-$(CONFIG_XILINX_HWICAP) += xilinx_hwicap
obj-$(CONFIG_XILINX_HWICAP) += xilinx_hwicap/xilinx_hwicap.o

include $(TOPDIR)/Rules.make

fastdep:
