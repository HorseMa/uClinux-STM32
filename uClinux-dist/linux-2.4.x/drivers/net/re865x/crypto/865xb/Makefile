#
# Makefile for the 865x software big number operation
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := 865xb_cryptoengine.o
obj-y    := \
			rtl8651b_authEngine.o \
			rtl8651b_cryptoEngine.o \
			sha1.o \
			hmac.o \
			md5.o \
			authSim.o \
			authTest.o \
			desSim.o \
			hmactest.o \
			md5test.o \
			randtest.o \
			sha1test.o \
			rand.o\
			aes_block.o \
			aes_core.o

#			destest.o \
#			des_setkey.o 
#			des_ecb.o \

obj-$(CONFIG_RTL865X_CLE) += cryptoCmd.o

EXTRA_CFLAGS += -I. -I../../rtl865x -I../../cle
EXTRA_CFLAGS += -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX


include $(TOPDIR)/Rules.make
