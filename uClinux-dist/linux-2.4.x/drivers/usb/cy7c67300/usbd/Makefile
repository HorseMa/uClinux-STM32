#
# Makefile for the USB serial device drivers.
#

O_TARGET	:= usbd_dev.o

# Object file lists.

obj-$(CONFIG_USB_CY7C67200_300)			+= usbd.o
obj-$(CONFIG_USB_CY7C67200_300)			+= usbd-func.o
obj-$(CONFIG_USB_CY7C67200_300)			+= usbd-bus.o
obj-$(CONFIG_USB_CY7C67200_300)			+= usbd-debug.o
obj-$(CONFIG_USB_CY7C67200_300)			+= ep0.o

# Objects that export symbols.
# export-objs	:= usbd_dev.o

subdir-$(CONFIG_USB_CY7C67200_300)		+= bi
subdir-$(CONFIG_USB_CY7C67200_300)		+= dedev

include $(TOPDIR)/Rules.make

