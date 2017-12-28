#
# arch/microblaze/Makefile
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

#
# If you want the kernel build to build modules outside of the tree
# then define this and pass it to the main linux makefile
#
ifdef EXTRA_MODULE_DIRS
SUBDIRS += $(EXTRA_MODULE_DIRS)
endif

ARCH_DIR = arch/$(ARCH)

# work out which gcc version we are using
GCC_VER = $(shell $(CC) -v 2>&1|grep version|cut -d " " -f 3)

# split into major and minor numbers
GCC_MAJOR = $(shell echo $(GCC_VER) | cut -d '.' -f 1)
GCC_MINOR = $(shell echo $(GCC_VER) | cut -d '.' -f 2)  

# setup memory model
ifdef CONFIG_MODEL_RAM
MODEL = ram
endif
ifdef CONFIG_MODEL_ROM
MODEL = rom
endif
export MODEL

# Find out which board/platform we are compiling for
include arch/$(ARCH)/Boards.mk

# get the compiler, flags and targets from the platform
# Microblaze is transitioning towards nice multi-platform build
# but we must not break old targets yet (Suzaku, mbvanilla_net)
ifdef PLATFORM

CFLAGS += -DPLATFORM=$(PLATFORM)

include $(ARCH_DIR)/platform/$(PLATFORM)/Rules.make

arch/$(ARCH)/empty.o:
	$(CROSS_COMPILE)gcc -o arch/$(ARCH)/empty.o -c arch/$(ARCH)/empty.c

bootstrap:
	@$(MAKEBOOT) bootstrap

archmrproper:

archdep:

else  ##### The old way!
# old way of doing things, for back-compatability
# it would be nice to delete this eventually 

GCC_DIR = $(shell $(CC) -v 2>&1 | grep specs | sed -e 's/.* \(.*\)specs/\1\./')

INCGCC = $(GCC_DIR)/include


# C library shenanigans.
# mb-gcc 2.95 split some of the libgcc stuff out into the standalone
# libc.a, with different versions of the libs for different HW mul/div/shift
# support.  We don't support this, instead just linking against bog-standard
# standalone libc.a.  This costs us performance.
#
# New compiler (mb-gcc-3.4.x) at least uses libgcc correctly, so we no longer
# have to link against standalone libc.a.  However, we must explicilty point
# the right, optimised version of libgcc.a.  

ifeq ($(GCC_MAJOR),2)
	LIBGCC = $(GCC_DIR)/libgcc.a
	LIBGCC += $(GCC_DIR)/../../../../microblaze/lib/libc.a
else
	# Build up path to correct libgcc.a
	LIBGCC := $(GCC_DIR)

	ifeq ($(CONFIG_XILINX_MICROBLAZE0_USE_BARREL),1)
		LIBGCC := $(LIBGCC)/bs
	endif

	ifeq ($(CONFIG_XILINX_MICROBLAZE0_USE_HW_MUL),1)
		LIBGCC := $(LIBGCC)/m
	endif

	LIBGCC := $(LIBGCC)/libgcc.a
endif

CFLAGS += -fno-builtin
CFLAGS += -DNO_MM -DNO_FPU -D__ELF__ -DMAGIC_ROM_PTR
CFLAGS += -D__linux__
CFLAGS += -I$(INCGCC)

ifdef CONFIG_MICROBLAZE_HARD_MULT
	CFLAGS += -mno-xl-soft-mul
endif

ifdef CONFIG_MICROBLAZE_HARD_DIV
	CFLAGS += -mno-xl-soft-div
endif

ifdef CONFIG_MICROBLAZE_HARD_BARREL
	CFLAGS += -mxl-barrel-shift
endif

HEAD := $(ARCH_DIR)/kernel/head.o

SUBDIRS += $(ARCH_DIR)/kernel $(ARCH_DIR)/mm $(ARCH_DIR)/lib $(ARCH_DIR)/xilinx_ocp

CORE_FILES := $(ARCH_DIR)/kernel/kernel.o $(ARCH_DIR)/mm/mm.o $(CORE_FILES)
CORE_FILES += $(ARCH_DIR)/xilinx_ocp/xilinx_ocp.o

LIBS += $(ARCH_DIR)/lib/lib.a $(LIBGCC)


# Deal with the initial contents of the blkmem device
ifdef ROOT_FS_IMAGE 
# Embed it in the kernel

ROOT_FS_IMAGE_OBJ = root_fs_image.o

CORE_FILES += $(ROOT_FS_IMAGE_OBJ)

$(LINUX) : $(ROOT_FS_IMAGE_OBJ)

# Note that we use the build-system's objcopy, as the microblaze tools 
# are fairly old, and don't have the --rename-section option.
$(ROOT_FS_IMAGE_OBJ): $(ROOT_FS_IMAGE)
	objcopy -I binary -O elf32-big --rename-section .data=.root,alloc,load,readonly,data,contents $< $@

endif # ROOT_FS_IMAGE


#kernel linker script
LD_SCRIPT := linux.ld

LINKFLAGS = -T $(LD_SCRIPT) 
$(LINUX) : $(LD_SCRIPT)
$(LD_SCRIPT): $(ARCH_DIR)/linux.ld.in
	$(CPP) -P -x assembler-with-cpp $(AFLAGS) $< > $@


bootstrap:
	@$(MAKEBOOT) bootstrap

archmrproper:

archdep:

archclean:
	rm -f $(LD_SCRIPT)
	rm -f $(LINUX)
	rm -f $(ARCH_DIR)/kernel/microblaze_defs.h $(ARCH_DIR)/kernel/microblaze_defs.d

endif #PLATFORM


