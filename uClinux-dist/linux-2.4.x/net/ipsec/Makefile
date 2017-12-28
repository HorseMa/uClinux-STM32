ifneq ($(CONFIG_KLIPS),)
.EXPORT_ALL_VARIABLES:

include $(ROOTDIR)/openswan/Makefile.ver

EXTRA_CFLAGS += -I$(ROOTDIR)/openswan -I$(ROOTDIR)/openswan/lib \
			-I$(ROOTDIR)/openswan/linux/net/ipsec \
			-I$(ROOTDIR)/openswan/linux/include \
			

vpath %.h $(ROOTDIR)/openswan/linux/net/ipsec

# special hack required because of exported object files
%.c: $(ROOTDIR)/openswan/linux/net/ipsec/%.c
	ln -fs $< $@
%.c: $(ROOTDIR)/openswan/linux/net/ipsec/aes/%.c
	ln -fs $< $@
%.c: $(ROOTDIR)/openswan/linux/net/ipsec/des/%.c
	ln -fs $< $@
%.S: $(ROOTDIR)/openswan/linux/net/ipsec/des/%.S
	ln -fs $< $@

include $(ROOTDIR)/openswan/linux/net/ipsec/Makefile.fs2_4

version.c:	version.in.c
	sed '/"/s/xxx/$(IPSECVERSION)/' version.in.c >$@

else

ifneq ($(CONFIG_IPSEC),)
.EXPORT_ALL_VARIABLES:

ifdef CONFIG_IXP4XX_CRYPTO	
EXTRA_CFLAGS += -I$(ROOTDIR)/freeswan/klips/net/ipsec_xscale -I$(ROOTDIR)/include -I$(ROOTDIR)
endif
EXTRA_CFLAGS += -I$(ROOTDIR)/freeswan -I$(ROOTDIR)/freeswan/lib \
			-I$(ROOTDIR)/freeswan/klips/net/ipsec \
			-I$(ROOTDIR)/freeswan/libdes

ifdef CONFIG_IXP4XX_CRYPTO
vpath %.h $(ROOTDIR)/freeswan/klips/net/ipsec_xscale:$(ROOTDIR)/freeswan/klips/net/ipsec
else
vpath %.h $(ROOTDIR)/freeswan/klips/net/ipsec
endif

ifdef CONFIG_IXP4XX_CRYPTO
%.c: $(ROOTDIR)/freeswan/klips/net/ipsec_xscale/%.c
	ln -fs $< $@
endif

# special hack required because of exported object files
%.c: $(ROOTDIR)/freeswan/klips/net/ipsec/%.c
	ln -fs $< $@

include $(ROOTDIR)/freeswan/klips/net/ipsec/Makefile

else

fastdep:

endif
endif
