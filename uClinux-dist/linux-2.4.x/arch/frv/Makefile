#
# frv/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (c) 2003, 2004 Red Hat Inc.
# - Written by David Howells <dhowells@redhat.com>
# - Derived from arch/m68knommu/Makefile,
#	Copyright (c) 1999,2001  D. Jeff Dionne <jeff@lineo.ca>,
#	Rt-Control Inc. / Lineo, Inc.
#
# Copyright (C) 1998,1999  D. Jeff Dionne <jeff@uclinux.org>,
#                          Kenneth Albanowski <kjahds@kjahds.com>,
#
# Based on arch/m68k/Makefile:
# Copyright (C) 1994 by Hamish Macdonald
#

ARCHDIR = arch/$(ARCH)

CCSPECS	:= $(shell $(CC) -v 2>&1 | grep "^Reading specs from " | head -1 | cut -c20-)
CCDIR	:= $(strip $(patsubst %/specs,%,$(CCSPECS)))
CPUCLASS := fr400

# test for cross compiling
COMPILE_ARCH = $(shell uname -m)

#
# If you want the kernel build to build modules outside of the tree
# then define this and pass it to the main linux makefile
#
ifdef EXTRA_MODULE_DIRS
SUBDIRS += $(EXTRA_MODULE_DIRS)
endif

UTS_SYSNAME = -DUTS_SYSNAME=\"uClinux\"

ARCHMODFLAGS	+= -G0 -mlong-calls
CFLAGS		+= -G4
LINKFLAGS	:= -X -T $(ARCHDIR)/vmlinux.ldi -Map linkmap.txt

ifdef CONFIG_GC_SECTIONS
CFLAGS		+= -ffunction-sections -fdata-sections
LINKFLAGS	+= --gc-sections
endif

ifndef CONFIG_UCLINUX
CFLAGS		+= -mcpu=fr450
AFLAGS		+= -mcpu=fr450
ASFLAGS		+= -mcpu=fr450
else
ifdef CONFIG_FR55x
CFLAGS		+= -mcpu=fr550 -DNO_MM
AFLAGS		+= -mcpu=fr550 -DNO_MM
ASFLAGS		+= -mcpu=fr550 -DNO_MM
else
CFLAGS		+= -mcpu=fr400 -DNO_MM
AFLAGS		+= -mcpu=fr400 -DNO_MM
ASFLAGS		+= -mcpu=fr400 -DNO_MM
endif
endif

ifndef CROSS_COMPILE
ifndef CONFIG_UCLINUX
CROSS_COMPILE = frv-linux-gnu-
else
ifdef CONFIG_FR55x
CROSS_COMPILE = frv550-uclinux-
else
CROSS_COMPILE = frv400-uclinux-
endif
endif
endif

# pretend the kernel is going to run on an FR400 with no media-fp unit
# - reserve CC3 for use with atomic ops
# - all the extra registers are dealt with only at context switch time
CFLAGS		+= -mno-fdpic -mgpr-32 -msoft-float -mno-media -ffixed-fcc3 -ffixed-cc3
AFLAGS		+= -mno-fdpic
ASFLAGS		+= -mno-fdpic

# Set up the memory model.  RAM or ROM.
ifdef CONFIG_RAMKERNEL
MODEL = ram
endif
ifdef CONFIG_ROMKERNEL
MODEL = rom
endif
ifdef CONFIG_HIMEMKERNEL
MODEL = himem
endif
export MODEL


HEAD := $(ARCHDIR)/kernel/head.o

SUBDIRS += $(ARCHDIR)/kernel $(ARCHDIR)/mm $(ARCHDIR)/lib

CORE_FILES := \
	$(ARCHDIR)/kernel/entry-table.o \
	$(ARCHDIR)/kernel/kernel.o \
	$(ARCHDIR)/mm/mm.o $(CORE_FILES)

ifdef CONFIG_MB93090_MB00
SUBDIRS		+= $(ARCHDIR)/mb93090-mb00
CORE_FILES	+= $(ARCHDIR)/mb93090-mb00/motherboard.o
endif

LIBS += $(ARCHDIR)/lib/lib.a 


vmlinux: $(ARCHDIR)/vmlinux.ldi
linux: $(ARCHDIR)/vmlinux.ldi

$(ARCHDIR)/vmlinux.ldi: $(ARCHDIR)/vmlinux.lds
	cpp -P -nostdinc -I include -D__ASSEMBLY__ -E -o $@ $<

bootstrap:
	@$(MAKEBOOT) bootstrap

archmrproper:
	@$(MAKE) -C $(ARCHDIR)/boot mrproper

archclean: FORCE
	@$(MAKE) -C $(ARCHDIR)/boot clean

archdep: scripts/mkdep symlinks
	@$(MAKE) -C $(ARCHDIR)/boot dep


.PHONY: FORCE
