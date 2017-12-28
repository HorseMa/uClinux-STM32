# arch/arm/mach-ks8695/Makefile
#
# Makefile for KS8695 architecture support
#

obj-y				:= cpu.o irq.o time.o gpio.o devices.o
obj-m				:=
obj-n				:=
obj-				:=

# PCI support is optional
obj-$(CONFIG_PCI)		+= pci.o

# Board-specific support
obj-$(CONFIG_MACH_KS8695)	+= board-micrel.o
obj-$(CONFIG_MACH_LITE300)	+= board-sg.o
obj-$(CONFIG_MACH_SG310)	+= board-sg.o
obj-$(CONFIG_MACH_PFW)		+= board-sg.o
obj-$(CONFIG_MACH_SE4200)	+= board-sg.o
obj-$(CONFIG_MACH_CM4002)	+= board-og.o
obj-$(CONFIG_MACH_CM4008)	+= board-og.o
obj-$(CONFIG_MACH_CM41xx)	+= board-og.o

