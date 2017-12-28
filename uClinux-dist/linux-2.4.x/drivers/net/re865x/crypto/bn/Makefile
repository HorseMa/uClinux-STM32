#
# Makefile for the 865x software big number operation
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := rtl865x_bn.o
obj-y    := 	bn_print.o \
	bn_lib.o \
	bn_word.o \
	bn_mul.o \
	bn_sqr.o \
	bn_add.o \
	bn_asm.o \
	bn_ctx.o \
	bn_shift.o \
	bn_div.o \
	bn_rand.o \
	bn_exp.o \
	bn_exp2.o \
	bn_mont.o \
	bn_gcd.o \
	bn_recp.o \
	bn_blind.o \
	bn_mpi.o \
	bn_prime.o \

EXTRA_CFLAGS += -I../../rtl865x
EXTRA_CFLAGS += -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX

include $(TOPDIR)/Rules.make
