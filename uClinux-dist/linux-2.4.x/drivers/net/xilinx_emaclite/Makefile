#
# Makefile for the Xilinx ethernet Lite driver
#

EXTRA_CFLAGS		+= -I$(TOPDIR)/arch/microblaze/xilinx_ocp

list-multi		:= xilinx_emaclite.o

# The Linux adapter for the Xilinx driver code.
xilinx_emaclite-objs	+= adapter.o

# The Xilinx OS independent code.
xilinx_emaclite-objs	+= xemaclite.o xemaclite_g.o xemaclite_intr.o xemaclite_l.o

obj-$(CONFIG_XILINX_EMACLITE) := xilinx_emaclite.o

xilinx_emaclite.o: $(xilinx_emaclite-objs)
	$(LD) -r -o $@ $(xilinx_emaclite-objs)

include $(TOPDIR)/Rules.make
