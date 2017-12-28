#
# Makefile for the Xilinx On Chip Peripheral support code
#

list-multi		:= xilinx_ocp.o

all: xilinx_ocp.o

USE_STANDARD_AS_RULE		:= TRUE

O_TARGET := xilinx_ocp.o

# The Xilinx OS independent code.
obj-y := xbasic_types.o xdma_channel.o xdma_channel_sg.o 	\
	xipif_v1_23_b.o xpacket_fifo_v2_00_a.o 			\
	xpacket_fifo_l_v2_00_a.o xversion.o			\
	xilinx_syms.o

# Linux file to EXPORT_SYMBOL all of the Xilinx entries.
export-objs		+= xilinx_syms.o

include $(TOPDIR)/Rules.make
