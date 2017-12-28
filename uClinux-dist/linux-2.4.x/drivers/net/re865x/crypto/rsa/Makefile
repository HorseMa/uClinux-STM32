#
# Makefile for the 865x software big number operation
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := rtl865x_rsa.o
obj-y    := 	rsa_eay.o \
	rsa_lib.o \
	rsa_pk1.o \
	rsa_ssl.o \
	rsa_none.o \
	rsa_gen.o \

EXTRA_CFLAGS += -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX
EXTRA_CFLAGS += -I../../rtl865x
EXTRA_CFLAGS += -I../../cle
EXTRA_CFLAGS += -I../


include $(TOPDIR)/Rules.make

