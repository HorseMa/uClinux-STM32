
O_TARGET := idedriver-arm.o

obj-y		:=
obj-m		:=

obj-$(CONFIG_BLK_DEV_IDE_ICSIDE)	+= icside.o
obj-$(CONFIG_BLK_DEV_IDE_RAPIDE)	+= rapide.o
obj-$(CONFIG_BLK_DEV_IDE_EP93XX)	+= ide-ep93xx.o
obj-$(CONFIG_BLK_DEV_IDE_ESS710)	+= ess710ide.o

EXTRA_CFLAGS	:= -I../

include $(TOPDIR)/Rules.make
