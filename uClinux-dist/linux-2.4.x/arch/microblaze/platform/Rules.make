#
# arch/microblaze/platform/Rules.make
#
# This file is included by the global makefile so that you can add your own
# platform-specific flags and dependencies.
# This is the generic platform Rules.make
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2004       Atmark Techno <yashi@atmark-techno.com>
# Copyright (C) 2004       John Williams <jwilliams@itee.uq.edu.au>
# Copyright (C) 1999,2001  Greg Ungerer (gerg@snapgear.com)
# Copyright (C) 1998,1999  D. Jeff Dionne <jeff@uClinux.org>
# Copyright (C) 1998       Kenneth Albanowski <kjahds@kjahds.com>
# Copyright (C) 1994 by Hamish Macdonald
# Copyright (C) 2000  Lineo Inc. (www.lineo.com)

GCC_DIR = $(shell $(CC) -v 2>&1 | grep specs | sed -e 's/.* \(.*\)specs/\1\./')

INCGCC = $(GCC_DIR)/include

ARCH_DIR = arch/$(ARCH)
PLAT_DIR = $(ARCH_DIR)/platform/$(PLATFORM)

CFLAGS += -O2
CFLAGS += -fno-builtin
CFLAGS += -DNO_MM -DNO_FPU -D__ELF__ -DMAGIC_ROM_PTR
CFLAGS += -D__linux__
CFLAGS += -I$(INCGCC)

# Build up CFLAGS to support various CPU options
ifeq ($(CONFIG_XILINX_MICROBLAZE0_USE_BARREL),1)
   CFLAGS += -mxl-barrel-shift
endif

ifeq ($(CONFIG_XILINX_MICROBLAZE0_USE_DIV),1)
    CFLAGS += -mno-xl-soft-div
else
    CFLAGS += -mxl-soft-div
endif

ifeq ($(CONFIG_XILINX_MICROBLAZE0_USE_HW_MUL),1)
    CFLAGS += -mno-xl-soft-mul
else
    CFLAGS += -mxl-soft-mul
endif

ifeq ($(CONFIG_XILINX_MICROBLAZE0_USE_PCMP_INSTR),1)
    CFLAGS += -mxl-pattern-compare
endif

LD_SCRIPT := linux.ld
LINKFLAGS = -T $(LD_SCRIPT)

# Find the correct libgcc.a
ifeq ($(GCC_MAJOR),2)
	LIBGCC = $(GCC_DIR)/libgcc.a
	LIBGCC += $(GCC_DIR)/../../../../microblaze/lib/libc.a
else
	# Much nicer!
	LIBGCC := $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
endif

#kernel linker script is preprocessed first
$(LINUX) : $(LD_SCRIPT)
$(LD_SCRIPT): $(PLAT_DIR)/$(MODEL).ld.in $(PLAT_DIR)/auto-config.in $(TOPDIR)/.config
	$(CPP) -P -x assembler-with-cpp $(AFLAGS) $< > $@

HEAD := $(ARCH_DIR)/platform/$(PLATFORM)/head_$(MODEL).o

SUBDIRS := $(ARCH_DIR)/kernel $(ARCH_DIR)/mm $(ARCH_DIR)/lib \
	   $(ARCH_DIR)/xilinx_ocp \
           $(ARCH_DIR)/platform/$(PLATFORM) $(SUBDIRS)

CORE_FILES := $(ARCH_DIR)/kernel/kernel.o $(ARCH_DIR)/mm/mm.o \
	    $(ARCH_DIR)/xilinx_ocp/xilinx_ocp.o \
            $(ARCH_DIR)/platform/$(PLATFORM)/platform.o $(CORE_FILES)

LIBS += $(ARCH_DIR)/lib/lib.a $(LIBGCC)

MAKEBOOT = $(MAKE) -C $(ARCH_DIR)/boot

archclean:
	@$(MAKEBOOT) clean
	rm -f $(LD_SCRIPT)
	rm -f $(ARCH_DIR)/platform/$(PLATFORM)/microblaze_defs.h

