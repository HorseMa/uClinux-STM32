#
#	Link Locale file
#
#
#
#
#

ifeq ($(CONFIG_RTL865X_LIGHT_ROMEDRV),y)
all:
	@echo "Light RomeDRV need not any log system"
	@exit 1
else
  ifeq ($(CONFIG_RTL865X_LOG_ZH_CN),y)
	CURR_LANG := zh_CN
  else
    ifeq ($(CONFIG_RTL865X_LOG_ZH_TW),y)
	CURR_LANG := zh_TW
    else
      ifeq ($(CONFIG_RTL865X_LOG_EN),y)
	CURR_LANG := en
      else
all:      
	@echo "Error!! Please choice a correct language for log system"
	@exit 1
      endif
    endif
  endif  
endif

all:		link
	@echo "make <all> OK."

fastdep:	link
	@echo "make <fastdep> OK."
ifeq ($(CONFIG_RTL865X_ROMEDRV),y)
	@touch ./log_curr.h
endif

clean:		unlink
	@echo "make <clean> OK."

link:
ifeq ($(CONFIG_RTL865X_ROMEDRV),y)
	ln -sf ${CURR_LANG}/log_curr.h ./
endif

unlink:
	rm -f ./log_curr.h

