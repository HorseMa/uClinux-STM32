#
# Makefile for the Xilinx PS/2 driver
#

list-multi		:= xilinx_keyb.o

# The code to expose the Xilinx PS/2 ports out as an 8042 keyboard controller
xilinx_keyb-objs	+= x8042.o
export-objs		+= x8042.o

# The Xilinx OS independent code.
xilinx_keyb-objs	+= xps2.o xps2_g.o xps2_intr.o xps2_l.o

obj-$(CONFIG_VT)	:= xilinx_keyb.o

xilinx_keyb.o: $(xilinx_keyb-objs)
	$(LD) -r -o $@ $(xilinx_keyb-objs)

include $(TOPDIR)/Rules.make
