#
# Makefile for the 865x High Performance
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...
# Note 3! Pay attention to the important comments line(21-25 and 42-46)!!!!!
O_TARGET := re865x.o

ifneq ($(CONFIG_RTL865X_MODULE_ROMEDRV),y)
obj-y := rtl_glue.o mmap.o flashdrv.o
else
obj-y := mmap.o flashdrv.o
EXTRA_CFLAGS += -DCONFIG_RTL865X_MODULE_INTERNAL
endif

export-objs += rtl_glue.o

  ifeq ($(CONFIG_RE865X),y)
	ifeq ($(CONFIG_RTL865XB),y)
		ifeq ($(CONFIG_RTL865X_LIGHT_ROMEDRV),y)
			#
			#	Configuration for Light Rome Driver
			#
			obj-$(CONFIG_RE865X) += lightrome/lightrome.o rtl865xb_swNic.o
			subdir-$(CONFIG_RE865X) := lightrome

		else 
			#
			#	The following configuration is for 865xB ROME driver.
			#
			obj-$(CONFIG_RE865X) += rtl865x/rtl865x.o rtl865xb_swNic.o
			ifeq ($(CONFIG_RTL865X_USR_DEFINE_TUNNEL),y)
				obj-$(CONFIG_RE865X) += rtl865x_usrDefineTunnelOp.o
			endif
			#
			#===>Attention: the following line define subdir with type ":=",not "+=", so to be
			#	    sure that the other addition to subdir should not be placed before this line,or 
			#	    the addition would be layed over!!
			#
			subdir-$(CONFIG_RE865X) := rtl865x                                  
			ifeq ($(CONFIG_RTL865X_MULTILAYER_BSP),y)
				obj-y += re_core.o rtl865x_proc.o rtl865x_log.o rtl865x_callBack.o
				obj-y += rtl865x_mpt.o
				obj-y += drvPlugIn/drvPlugIn.o
				subdir-y += drvPlugIn
				obj-$(CONFIG_RTL865XB_GLUEDEV) += rtl865x_glueDev.o
				obj-y += rtl8651_ipcomp.o
				EXTRA_CFLAGS += -I.
			else
				obj-y += re865x_nic.o
			endif
		endif

	else
	ifeq ($(CONFIG_RTL865XC),y)
	  ifeq ($(CONFIG_RTL865X_LIGHT_ROMEDRV),y)
			#
			#	Configuration for Light Rome Driver
			#
			obj-$(CONFIG_RE865X) += lightrome/lightrome.o rtl865xc_swNic.o
			subdir-$(CONFIG_RE865X) := lightrome

		else
		#
		#	The following configuration is used for 865xC model code only.
		#	In fact, we will port more and more driver files from 865xB to 865xC.
		#
		EXTRA_CFLAGS += 
		obj-$(CONFIG_RE865X) += rtl865x/rtl865x.o rtl865xc_swNic.o
		ifeq ($(CONFIG_RTL865X_USR_DEFINE_TUNNEL),y)
                                  obj-$(CONFIG_RE865X) += rtl865x_usrDefineTunnelOp.o
		endif
		#
		#===>Attention: the following line define subdir with type ":=",not "+=", so to be
		#	    sure that the other addition to subdir should not be placed before this line,or 
		#	    the addition would be layed over!!
		#
		subdir-$(CONFIG_RE865X) := rtl865x
		ifeq ($(CONFIG_RTL865X_MULTILAYER_BSP),y)
			obj-y += re_core.o rtl865x_proc.o rtl865x_log.o rtl865x_callBack.o
			obj-y += rtl865x_mpt.o
			obj-y += drvPlugIn/drvPlugIn.o
			subdir-y += drvPlugIn
			obj-$(CONFIG_RTL865XB_GLUEDEV) += rtl865x_glueDev.o
			obj-y += rtl8651_ipcomp.o
			EXTRA_CFLAGS += -I.
		else
			obj-y += re865x_nic.o
		endif
	 endif 
	endif
	endif
  endif

  subdir-y += locale

  obj-$(CONFIG_RTL865X_CLE) += cle/cle.o cle_cmdRoot.o
  subdir-$(CONFIG_RTL865X_CLE) += cle 
  ifeq ($(CONFIG_RTL865X_CLE),y)
     export-objs += cle_cmdRoot.o
  endif

  obj-$(CONFIG_RTL865X_ROMEREAL) += romereal.o
  obj-$(CONFIG_RTL865X_ROMEPERF) += romeperf.o
ifneq ($(CONFIG_RTL865X_LIGHT_ROMEDRV),y)
  obj-$(CONFIG_RTL865XB_EXP_CRYPTOENGINE) += crypto/re865x_crypto.o

  ifeq ($(CONFIG_IGMP_SNOOPING), y)  
	EXTRA_CFLAGS += -I./rtl8306/rtl8306sdm_multicast
       EXTRA_CFLAGS += -I./rtl8306/rtl8306sdm_otherTrapPkt
	EXTRA_CFLAGS += -I./rtl865x
    obj-y += rtl8306/rtl8306sdm_multicast/rtl8306sdm_multicast.o	
    obj-y += rtl8306/rtl8306sdm_otherTrapPkt/rtl8306sdm_otherTrapPkt.o
    subdir-y += rtl8306/rtl8306sdm_multicast
    subdir-y += rtl8306/rtl8306sdm_otherTrapPkt
  endif
 
ifeq ($(CONFIG_MULTICAST_SNOOPING), y)  
	EXTRA_CFLAGS += -I./rtl8306/rtl8306sdm_multicastV2
	EXTRA_CFLAGS += -I./rtl8306/rtl8306sdm_otherTrapPkt
	EXTRA_CFLAGS += -I./rtl865x
   obj-y += rtl8306/rtl8306sdm_multicastV2/rtl8306sdm_multicastV2.o 
   obj-y += rtl8306/rtl8306sdm_otherTrapPkt/rtl8306sdm_otherTrapPkt.o
   subdir-y += rtl8306/rtl8306sdm_multicastV2
   subdir-y += rtl8306/rtl8306sdm_otherTrapPkt
endif
 

subdir-$(CONFIG_RTL865XB_EXP_CRYPTOENGINE) += crypto

subdir-$(CONFIG_RTL865XC) += gpio
obj-$(CONFIG_RTL865XC) += gpio/gpio.o

# model code
  ifeq ($(CONFIG_RTL865X_MODEL_KERNEL),y)
	subdir-y += gdma
	subdir-y += model
	subdir-y += model/865xC
	subdir-y += rtl865x/tblDrvTest/PktProc
	obj-y += model/model.o
	obj-y += model/865xC/865xC.o
	obj-y += gdma/gdma.o
	obj-y += rtl865x/tblDrvTest/PktProc/PktProc.o
	EXTRA_CFLAGS += -DRTL865X_MODEL_KERNEL
	EXTRA_CFLAGS += -Icle
  endif

endif
EXTRA_CFLAGS += -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX -D__KERNEL__ -Wno-implicit
#-Werror
ifeq ($(CONFIG_AIRGO103BB),y)
EXTRA_CFLAGS += -DAIRGO_FAST_PATH
#EXTRA_CFLAGS += -DAIRGO_NEW
endif
#EXTRA_CFLAGS += -DRTL865X_DEBUG

ifeq ($(CONFIG_RTL8169_FASTPATH),y)
EXTRA_CFLAGS += -DAIRGO_FAST_PATH
endif

ifeq ($(CONFIG_RTL8139_FASTPATH),y)
EXTRA_CFLAGS += -DAIRGO_FAST_PATH
endif

#obj-m   := $(O_TARGET)


	EXTRA_CFLAGS += -D__KERNEL__ 
	EXTRA_CFLAGS += -DEXPORT_SYMTAB 
ifeq ($(CONFIG_RTL865X_MODULE_ROMEDRV),y)
	EXTRA_CFLAGS += -G 0
	EXTRA_CFLAGS += -DMODULE
	EXTRA_CFLAGS += -mlong-calls
#	EXTRA_CFLAGS += -DMODVERSIONS 
#	EXTRA_CFLAGS += -include $(ROOTDIR)/$(LINUXDIR)/include/linux/modversions.h
endif		#$(CONFIG_RTL865X_MODULE_ROMEDRV)!=y

include $(TOPDIR)/Rules.make

flashdrv.o: board.h
board.h:
	ln -fs rtl_board.h board.h
