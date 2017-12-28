#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= at91rm9200.o

# Object file lists.

obj-y			:= core.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:=

# LEDs support
leds-$(CONFIG_ARCH_AT91RM9200DK) += dk-leds.o
obj-$(CONFIG_LEDS) += $(leds-y)

include $(TOPDIR)/Rules.make
