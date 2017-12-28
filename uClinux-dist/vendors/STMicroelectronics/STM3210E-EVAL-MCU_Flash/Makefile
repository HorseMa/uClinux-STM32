include $(LINUX_CONFIG)
include $(CONFIG_CONFIG)
include $(ARCH_CONFIG)

LINUXFILE = xipImage

LINUXIMG  = $(IMAGEDIR)/$(LINUXFILE).bin


ROMFS_DIRS = \
	bin \
	dev \
	etc \
	proc \
	sys
clean:
	-rm -rf $(LINUXDIR)/gen-initramfs-filelist.sh
romfs:
	[ -d $(ROMFSDIR)/$$i ] || mkdir -p $(ROMFSDIR)
	for i in $(ROMFS_DIRS); do \
		[ -d $(ROMFSDIR)/$$i ] || mkdir -p $(ROMFSDIR)/$$i; \
	done
	cp ./motd $(ROMFSDIR)/etc/
	cp ./linuxrc $(ROMFSDIR)/
	chmod +x ./gen-initramfs-filelist.sh
romfs.post::
	./gen-initramfs-filelist.sh $(ROMFSDIR)/

.PHONY: image
image: 
	[ -d $(IMAGEDIR) ] || mkdir -p $(IMAGEDIR)
	cp $(ROOTDIR)/$(LINUXDIR)/arch/arm/boot/$(LINUXFILE) $(LINUXIMG)	