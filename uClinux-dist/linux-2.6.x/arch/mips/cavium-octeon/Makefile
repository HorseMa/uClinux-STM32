#
# Makefile for the Cavium Octeon specific kernel interface routines
# under Linux.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2005-2007 Cavium Networks
#

OCTEON_ROOT = $(srctree)/arch/mips/cavium-octeon
EXTRA_CFLAGS +=	-I$(OCTEON_ROOT)/executive/config \
		-I$(OCTEON_ROOT)/executive

obj-y := setup.o serial.o irq.o hal.o
obj-y += dma-octeon.o userio.o
obj-y += octeon-memcpy.o
obj-y += console.o
obj-y += gpio.o
#obj-y += flash_setup.o

obj-$(CONFIG_SMP) += smp.o
obj-$(CONFIG_PCI) += pci.o pci-common.o
