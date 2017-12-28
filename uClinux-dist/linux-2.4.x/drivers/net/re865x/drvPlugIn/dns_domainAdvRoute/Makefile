#
# Makefile for domainAdvRoute
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := dns_domainAdvRoute.o

obj-y += rtl8651_dns_domainAdvRoute.o

EXTRA_CFLAGS += -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX -D__KERNEL__ -Werror -g


include $(TOPDIR)/Rules.make
