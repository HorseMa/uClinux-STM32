#
# Makefile for the Tulip ethernet driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := rtl8186.o

obj-$(CONFIG_RTL865X_CLE) += cle/cle.o cle_cmdRoot.o
obj-y += cle/cle.o cle_cmdRoot.o pcm.o i2c.o
subdir-$(CONFIG_RTL865X_CLE) += cle 
subdir-y += cle 
ifeq ($(CONFIG_RTL865X_CLE),y)
   export-objs += cle_cmdRoot.o
endif

EXTRA_CFLAGS += -DRTL865X_DEBUG -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX -D__KERNEL__ -Wno-implicit -Werror



#obj-m   := $(O_TARGET)


include $(TOPDIR)/Rules.make

cle/cle.o:  cle
cle:
	ln -fs ../re865x/cle .
	
fastdep: cle
