#
# Makefile for the GPIO driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := gpio.o

obj-y := \
		rtl865xc_gpio.o 
		
ifeq ($(CONFIG_RTL865X_CLE),y)
obj-y +=	rtl865xc_gpio_cmd.o 
endif
			
export-objs += 

obj-$(CONFIG_RTL865X_MODEL_KERNEL) += 
subdir-$(CONFIG_RTL865X_MODEL_KERNEL) += 

all_targets: check_obj

check_obj:
	file $(obj-y) | grep Intel | awk -F: '{print $$1};' | xargs rm -f --

EXTRA_CFLAGS += -Wall -ggdb -Werror
EXTRA_CFLAGS += -DCONFIG_RTL865X -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX
EXTRA_CFLAGS += -I. -I../rtl865x/ -I../cle/ -I../
ifeq ($(CONFIG_RTL865X_MODEL_KERNEL),y)
	EXTRA_CFLAGS += -DRTL865X_MODEL_KERNEL
endif


include $(TOPDIR)/Rules.make
