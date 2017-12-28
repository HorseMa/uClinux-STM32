
# Makefile for the Tulip ethernet driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := rtl865x.o

#
# We decide the object file's layout before construct it.
#
ifeq ($(CONFIG_RTL865X_MULTILAYER_BSP),y)
	# L3/4 HW acceleration
else
	# Pure L2 BSP
	MAKE_RTL865X_L2_MODE := _L2
endif

ifeq ($(CONFIG_RTL865X_CLE),y)
	# CLE enabled
	MAKE_RTL865X_CLE := CLE
endif

ifeq ($(CONFIG_RTL865X_IPSEC),y)
	# IPSec enabled
	MAKE_RTL865X_IPSEC := _IPSEC
endif

MAKE_OBJFILE := rtl865xB_$(ROMEDRIVER_VERSION)_FWD$(MAKE_RTL865X_CLE)$(MAKE_RTL865X_IPSEC)$(MAKE_RTL865X_L2_MODE).obj

ifeq ($(SDK_PACKAGE),y)

       obj-y := $(MAKE_OBJFILE)

else	#Not SDK_PACKAGE, Realtek internal

	ifeq ($(CONFIG_RTL865XB),y)
		#
		#	The following configuration is for 865xB ROME driver.
		#
		ROME_OBJS    := mbuf.o \
					rtl_utils.o \
					rtl865xB_tblDrvPatch.o \
					rtl8651_tblBasicDrv.o \
					rtl8651_layer2asic.o\
					rtl8651_layer2.o\
					rtl8651_tblAsicDrv.o
	endif

	ifeq ($(CONFIG_RTL865X_MULTILAYER_BSP),y)
		ROME_OBJS +=  \
			rtl8651_tblDrvFwd.o \
			rtl8651_tblDrv.o \
			rtl8651_layer2fwd.o\
			rtl8651_ipQueue.o \
			rtl8651_multicast.o \
			rtl8651_timer.o \
			rtl8651_dos.o \
			rtl8651_dos_proc.o \
			rtl8651_dns.o \
			rtl8651_dns_domainBlock.o \
			rtl8651_alg_init.o \
			rtl8651_alg_ftp.o \
			rtl8651_alg_mms.o \
			rtl8651_alg_h323.o \
			rtl8651_alg_dx7play.o \
			rtl8651_alg_pptp.o \
			rtl8651_alg_smedia.o \
			rtl8651_alg_ipsec.o \
			rtl8651_alg_ipcomp.o \
			rtl8651_alg_ymsg.o \
			rtl8651_alg_icq.o \
			rtl8651_alg_irc_dcc.o \
			rtl8651_alg_l2tp.o \
			rtl8651_alg_sip.o \
			rtl8651_alg_quake.o \
			rtl8651_alg_cs.o \
			rtl8651_alg_vdolive.o \
			rtl8651_alg_battlenet.o \
			rtl8651_alg_realaudio.o \
			rtl8651_alg_rtsp.o \
			rtl8651_alg_cuseeme.o \
			rtl8651_alg_tftp.o \
			rtl8651_alg_qos.o

		ifeq ($(CONFIG_RTL865X_PPP),y)
			ROME_OBJS += rtl_ppp.o rtl_pppoe.o md5.o
		endif

		ifeq ($(CONFIG_RTL8305S),y) 
			ROME_OBJS += rtl8305s.o 

			ifeq ($(CONFIG_RTL865X_CLE),y)
				ROME_OBJS += rtl8305s_cmd.o
			endif         
		endif

		ifeq ($(CONFIG_RTL8305SC), y)
			ROME_OBJS += rtl8305sc.o
		endif

		ifeq ($(CONFIG_RTL8305SB), y)
			ROME_OBJS += rtl8305sb.o
		endif

		ifeq ($(CONFIG_RTL865X_CLE),y)
			ROME_OBJS += rtl8651_tblDrvStatistic.o  rtl8651_tblAsicDrv_cmd.o rtl8651_tblDrv_cmd.o rtl8651_tblDrvFwd_cmd.o
			EXTRA_CFLAGS += -I../cle	
		endif

		ifeq ($(CONFIG_RTL865X_IPSEC),y)
			ROME_OBJS += rtl8651_ipsec.o
			EXTRA_CFLAGS += -I../crypto/865xb/
		endif

		ifeq ($(CONFIG_RTL865X_VOIP),y)
			ROME_OBJS += voip_support.o
		endif

		ifeq ($(CONFIG_IGMP_SNOOPING), y)  
			EXTRA_CFLAGS += -I../rtl8306/rtl8306sdm_multicast
			EXTRA_CFLAGS += -I../rtl8306/rtl8306sdm_otherTrapPkt
		endif
		
		ifeq ($(CONFIG_MULTICAST_SNOOPING), y)  
			EXTRA_CFLAGS += -I../rtl8306/rtl8306sdm_multicastV2
                     EXTRA_CFLAGS += -I../rtl8306/rtl8306sdm_otherTrapPkt
		endif

                ifeq ($(CONFIG_RTL865X_MODEL_KERNEL),y)
                        EXTRA_CFLAGS += -DRTL865X_MODEL_KERNEL
                else
		  ifeq ($(CONFIG_RTL865XB),y)
			ROME_OBJS += hs.o
		  endif
                endif
	else
		ifeq ($(CONFIG_RTL865XB),y)
			subdir-$(CONFIG_RTL865XB_EXP_VERI) += veri
			obj-$(CONFIG_RTL865XB_EXP_VERI) += veri/veripkg.o

			ifeq ($(CONFIG_RTL865X_MODEL_KERNEL),y)
				EXTRA_CFLAGS += -DRTL865X_MODEL_KERNEL
			else
				ifeq ($(CONFIG_RTL865XB),y)
					ROME_OBJS += hs.o
				endif
			endif
		endif
	endif

	obj-y	:=$(ROME_OBJS)

endif #SDK_PACKAGE

EXTRA_CFLAGS += -O1 -DRTL_TBLDRV -D__linux__  -mno-memcpy -DRTL865X_OVER_KERNEL -DRTL865X_OVER_LINUX -Wno-implicit -Werror
#EXTRA_CFLAGS += -DRTL865X_DEBUG 
EXTRA_CFLAGS += -I.


ifeq ($(CONFIG_RTL865X_MODULE_ROMEDRV),y)
	EXTRA_CFLAGS += -D__KERNEL__ 
	EXTRA_CFLAGS += -G 0
	EXTRA_CFLAGS += -DMODULE
	EXTRA_CFLAGS += -mlong-calls
	EXTRA_CFLAGS += -DEXPORT_SYMTAB 
	EXTRA_CFLAGS += -DCONFIG_RTL865X_MODULE_INTERNAL
#	EXTRA_CFLAGS += -DMODVERSIONS 
#	EXTRA_CFLAGS += -include $(ROOTDIR)/$(LINUXDIR)/include/linux/modversions.h
endif

include $(TOPDIR)/Rules.make
