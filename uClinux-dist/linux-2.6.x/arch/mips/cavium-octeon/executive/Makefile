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


source:=$(srctree)/$(src)
EXTRA_CFLAGS	+= -I$(source) -I$(source)/config
EXTRA_CFLAGS	+= -I${OCTEON_ROOT}/target/include

executive-files := cvmx-bootmem.o
executive-files += cvmx-cmd-queue.o
executive-files += cvmx-fpa.o
executive-files += cvmx-helper.o
executive-files += cvmx-helper-board.o
executive-files += cvmx-helper-errata.o
executive-files += cvmx-helper-fpa.o
executive-files += cvmx-helper-loop.o
executive-files += cvmx-helper-npi.o
executive-files += cvmx-helper-rgmii.o
executive-files += cvmx-helper-sgmii.o
executive-files += cvmx-helper-spi.o
executive-files += cvmx-helper-util.o
executive-files += cvmx-helper-xaui.o
executive-files += cvmx-l2c.o
executive-files += cvmx-pko.o
executive-files += cvmx-spi.o
executive-files += cvmx-spi4000.o
executive-files += cvmx-sysinfo.o
executive-files += cvmx-warn.o
executive-files += octeon-model.o
executive-files += cvmx-linux-kernel-exports.o
obj-y := $(executive-files)

executive-obj-files := $(executive-files:%=$(obj)/%)
executive-src-files := $(executive-obj-files:%.o=%.c)

