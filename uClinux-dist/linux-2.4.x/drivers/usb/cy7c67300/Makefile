#
# Makefile for the USB serial device drivers.
#

O_TARGET    := cy7c67300.o

# Object file lists.

obj-$(CONFIG_USB_CY7C67200_300)                 += cy7c67200_300_hcd.o
obj-$(CONFIG_USB_CY7C67200_300)                 += cy7c67200_300_hcd_simple.o
obj-$(CONFIG_USB_CY7C67200_300)                 += cy7c67200_300_hcd_rh.o
obj-$(CONFIG_USB_CY7C67200_300)                 += cy7c67200_300_otg.o
obj-$(CONFIG_USB_CY7C67200_300)                 += cy7c67200_300_lcd.o

# Objects that export symbols.
# export-objs   := usbserial.o

# Don't build broken dedvr example code
subdir-$(CONFIG_USB_CY7C67200_300)      += usbd
#subdir-$(CONFIG_USB_CY7C67200_300)      += dedrv

include $(TOPDIR)/Rules.make

