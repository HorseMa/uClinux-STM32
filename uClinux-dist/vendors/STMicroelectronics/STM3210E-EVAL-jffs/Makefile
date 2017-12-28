include $(LINUX_CONFIG)
include $(CONFIG_CONFIG)
include $(ARCH_CONFIG)

LINUXFILE = xipImage
JFFS2FILE = rootfs.img

LINUXIMG  = $(IMAGEDIR)/$(LINUXFILE).bin
JFFS2IMG  = $(IMAGEDIR)/$(JFFS2FILE).bin

DIRS = 

ROMFS_DIRS = \
	bin \
	dev \
	etc \
	home\
	lib \
	mnt \
	proc \
	root \
	sbin \
	sys \
	usr usr/bin
	
all:
	dirs=$(DIRS) ; \
	for i in $$dirs ; do  make -C $$i || exit $? ; done

clean:
	-dirs=$(DIRS) ; \
	make -C ./Splash/ clean;\
	for i in $$dirs; do [ ! -d $$i ] || make -C $$i clean; done;

romfs:
	[ -d $(ROMFSDIR)/$$i ] || mkdir -p $(ROMFSDIR)
	for i in $(ROMFS_DIRS); do \
		[ -d $(ROMFSDIR)/$$i ] || mkdir -p $(ROMFSDIR)/$$i; \
	done
	dirs=$(DIRS) ; for i in $$dirs; do  make -C $$i romfs || exit $? ; done
	make -C ./Splash all romfs
#	$(ROMFSINST) ./Splash/Splash /bin/Splash
	$(ROMFSINST) ./linuxrc /linuxrc
	$(ROMFSINST) ./motd /etc/motd
	$(ROMFSINST) ./Logo_uCLinux.bmp /root
	$(ROMFSINST) -p 755 /linuxrc
	$(ROMFSINST) -p 755 /bin/Splash
	
build_host_jffs2:
	make -C $(ROOTDIR)/user/mtd-utils host_mkfs.jffs2
	
$(JFFS2IMG):	
	$(ROOTDIR)/user/mtd-utils/build/mkfs.jffs2 -D device.tab -o $(JFFS2IMG) -q -x rtime -x zlib -d $(ROMFSDIR)
	
.PHONY: image	build_host_jffs2 $(JFFS2IMG) 
image: build_host_jffs2 $(JFFS2IMG) 
	[ -d $(IMAGEDIR) ] || mkdir -p $(IMAGEDIR)
	cp $(ROOTDIR)/$(LINUXDIR)/arch/arm/boot/$(LINUXFILE) $(LINUXIMG)
	cp ./Logo_uCLinux.bmp $(IMAGEDIR)/logo.bin
	