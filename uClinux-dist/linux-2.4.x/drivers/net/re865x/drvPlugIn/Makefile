#
# Makefile for direction drvPlugIn
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := drvPlugIn.o


obj-y += dns_domainAdvRoute/dns_domainAdvRoute.o
subdir-y += dns_domainAdvRoute



EXTRA_CFLAGS += -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX -D__KERNEL__ -Werror -g



include $(TOPDIR)/Rules.make
