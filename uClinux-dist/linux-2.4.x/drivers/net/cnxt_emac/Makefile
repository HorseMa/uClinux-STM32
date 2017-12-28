# File: drivers/net/cnxt_emac/Makefile
#
# Makefile for the Linux Conexant emac driver.
#

O_TARGET := emac_drv.o

EXTRA_CFLAGS += -DOS_LINUX -DLINUX_EMBEDDED	-DLINUX_DRIVER

obj-y += cnxtEmac.o \
	 phy.o \
	 mii.o
# JH: does not compile, disabled:	 tesla.o

obj-m := $(O_TARGET)

include $(TOPDIR)/Rules.make
