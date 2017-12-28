#
# Makefile for the kernel spi bus driver.
#

O_TARGET := spi.o

export-objs	:= spi_char.o spi_mcf.o m41t94.o

#obj-$(CONFIG_SPI)		+= spi_char.o spi_core.o

# SPI character driver
obj-$(CONFIG_SPI_CHAR)		+= spi_char.o

# M41T94 RTC/WD driver
obj-$(CONFIG_SPI_M41T94)	+= m41t94.o

# SPI adapters
obj-$(CONFIG_SPI_MCF)		+= spi_mcf.o

include $(TOPDIR)/Rules.make

