#
# Makefile for RTL865X command line engine
#

O_TARGET := cle.o

ifeq ($(SDK_PACKAGE),n)
CLE_OBJS := \
	cle_ciscoParser.o \
	cle_ciscoParserParamChecker.o \
	cle_parser.o cle_utility.o \
	cmd_keyword.o \
	cmd_cmdEveryWhere.o \
	cle_linuxglue.o \

ifeq ($(CONFIG_RE865X),)
	CLE_OBJS += rtl_utils.o 
endif

EXTRA_CFLAGS += -I.. -I../rtl865x -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX
obj-$(CONFIG_RTL865X_CLE) :=  $(CLE_OBJS)
else
obj-$(CONFIG_RTL865X_CLE) := cle-$(ROMEDRIVER_VERSION).obj
obj-m := cle-$(ROMEDRIVER_VERSION).obj
endif


ifeq ($(CONFIG_RTL865X_MODULE_ROMEDRV),y)
	EXTRA_CFLAGS += -D__KERNEL__ 
	EXTRA_CFLAGS += -G 0
	EXTRA_CFLAGS += -DMODULE
	EXTRA_CFLAGS += -mlong-calls
	EXTRA_CFLAGS += -DEXPORT_SYMTAB 
#	EXTRA_CFLAGS += -DMODVERSIONS 
#	EXTRA_CFLAGS += -include $(ROOTDIR)/$(LINUXDIR)/include/linux/modversions.h
endif


# Hand off to Rules.make.

include $(TOPDIR)/Rules.make
