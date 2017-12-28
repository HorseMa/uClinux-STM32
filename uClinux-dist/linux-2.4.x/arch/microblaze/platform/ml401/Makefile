#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

#
# re-use any microblaze files that we can.  Perhaps we should create
# a microblaze directory for shared files ?

VPATH := $(VPATH):../../kernel

ifdef CONFIG_FULLDEBUG
	AFLAGS += -DDEBUGGER_COMPATIBLE_CACHE=1
endif

# Work out special CPU flags needed

.S.o:
	$(CC) -D__ASSEMBLY__ $(AFLAGS) -I. -c $< -o $*.o

all: head_$(MODEL).o entry.o platform.o

O_TARGET := platform.o

obj-y := intv.o entry.o machine.o setup.o

head_$(MODEL).o: head_$(MODEL).S

entry.o: entry.S microblaze_defs.h

microblaze_defs.h: ../../kernel/microblaze_defs.c ../../kernel/microblaze_defs.head
	rm -f microblaze_defs.d
	$(CC) $(filter-out -MD,$(CFLAGS)) -S ../../kernel/microblaze_defs.c
	cp ../../kernel/microblaze_defs.head microblaze_defs.h
	grep '^#define' microblaze_defs.s >> microblaze_defs.h
	rm microblaze_defs.s
-include microblaze_defs.d

include $(TOPDIR)/Rules.make
