#
# Makefile for Fujitsu-specific device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs	:=  mb93493-audio.o

# The target object and module list name.

O_TARGET	:= fujitsu.o

# Object file lists.

obj-y	:= mb93493.o
obj-m	:=
obj-n	:=
obj-	:=

# Each configuration option enables a list of files.

obj-$(CONFIG_FUJITSU_MB93493_VDC)	+= mb93493-vdc.o
obj-$(CONFIG_FUJITSU_MB93493_VCC)	+= mb93493-vcc.o
obj-$(CONFIG_FUJITSU_MB93493_I2C)	+= mb93493-i2c.o
obj-$(CONFIG_FUJITSU_MB93493_I2S)	+= mb93493-audio.o

# The global Rules.make.

include $(TOPDIR)/Rules.make

