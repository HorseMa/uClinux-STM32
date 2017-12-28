#
# Makefile for Xilinx HWICAP driver
#

EXTRA_CFLAGS		+= -I$(TOPDIR)/arch/microblaze/xilinx_ocp

# the uClinux adapter for the xilinx driver code
xilinx_hwicap-objs	+= adapter.o

# The Xilinx OS independent code
xilinx_hwicap-objs	+= xhwicap_device_read_frame.o xhwicap_g.o \
			   xhwicap_srp.o xhwicap_device_write_frame.o \
			   xhwicap_set_configuration.o

obj-$(CONFIG_XILINX_HWICAP) := xilinx_hwicap.o

xilinx_hwicap.o: $(xilinx_hwicap-objs)
	$(LD) -r -o $@ $(xilinx_hwicap-objs)

include $(TOPDIR)/Rules.make

