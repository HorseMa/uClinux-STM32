#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

USE_STANDARD_AS_RULE := true

O_TARGET		:= S3C3410.o

# Object file lists.

obj-y			:= $(patsubst %.c, %.o, $(wildcard *.c))
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= 

include $(TOPDIR)/Rules.make
