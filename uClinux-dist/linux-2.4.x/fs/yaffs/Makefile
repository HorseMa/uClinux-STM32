#
# Makefile for the linux YAFFS filesystem routines.
#

list-multi := yaffs.o

yaffs-objs := yaffs_ecc.o yaffs_fs.o yaffs_guts.o

ifeq ($(CONFIG_YAFFS_MTD_ENABLED),y)
	yaffs-objs += yaffs_mtdif.o
endif

ifeq ($(CONFIG_YAFFS_RAM_ENABLED),y)
	yaffs-objs += yaffs_ramem.o
endif

O_TARGET := yaffs.o

obj-y := $(yaffs-objs)
obj-m := $(O_TARGET)

include $(TOPDIR)/Rules.make
