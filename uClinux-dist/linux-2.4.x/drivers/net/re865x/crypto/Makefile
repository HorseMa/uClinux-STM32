#
# Makefile for the Tulip ethernet driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := re865x_crypto.o
subdir-y := 
obj-y    :=

export-objs = 

ifeq ($(CONFIG_RTL865XB_EXP_CRYPTOENGINE),y)
	ifeq ($(CONFIG_RTL865XB),y)
		obj-y += 865xb/865xb_cryptoengine.o
		subdir-y += 865xb
	else
	ifeq ($(CONFIG_RTL865XC),y)
		obj-y += 865xc/865xc_cryptoengine.o
		subdir-y += 865xc
	endif
	endif
endif

ifeq ($(CONFIG_RTL865x_SWPUBKEY),y)
  obj-y += rtl_crypt_cmd.o
  obj-y += bn/rtl865x_bn.o rsa/rtl865x_rsa.o dh/rtl865x_dh.o
  subdir-y += bn dh rsa
endif

EXTRA_CFLAGS += -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX

#obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
