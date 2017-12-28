#
# Makefile for the linux squashfs routines.
#

O_TARGET := squashfs.o

obj-y  := inode.o
ifdef CONFIG_SQUASHFS_LZMA
obj-y  += LzmaDecode.o LzmaWrapper.o
endif

obj-m := $(O_TARGET)

include $(TOPDIR)/Rules.make
