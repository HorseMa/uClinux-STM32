
CFLAGS += -DNO_GETTEXT -DHAVE_ISC_READER -DDO_PRELOAD

CFLAGS += -g -O2
ifndef CONFIG_IPV6
CFLAGS += -DNO_IPV6
endif
ifdef CONFIG_USER_FLATFSD_FLATFSD
CFLAGS += -DETC_DIR=\"/etc/config\"
endif

EXTRA_OBJS = preload.o
export EXTRA_OBJS

all: 
	$(MAKE) -f Makefile all

clean :
	$(MAKE) -f Makefile clean

install : 
	$(MAKE) -f Makefile install

romfs:
	$(ROMFSINST) src/dnsmasq /bin/dnsmasq
