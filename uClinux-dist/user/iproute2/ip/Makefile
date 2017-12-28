IPOBJ=ip.o ipaddress.o iproute.o iprule.o \
    rtm_map.o iptunnel.o tunnel.o ipneigh.o ipntable.o iplink.o \
    ipmaddr.o ipmonitor.o ipmroute.o ipprefix.o \
    ipxfrm.o xfrm_state.o xfrm_policy.o xfrm_monitor.o \
    iplink_vlan.o link_veth.o
ifdef CONFIG_IPV6
IPOBJ+=ip6tunnel.o
endif

RTMONOBJ=rtmon.o

ALLOBJ=$(IPOBJ) $(RTMONOBJ)
SCRIPTS=ifcfg rtpr routel routef
TARGETS=ip rtmon

all: $(TARGETS) $(SCRIPTS) $(LIBS)

ip: $(IPOBJ) $(LIBNETLINK)
	$(CC) $(LDFLAGS) -o $@ $(IPOBJ) $(LIBNETLINK) $(LDLIBS$(LDLIBS_$@))

rtmon: $(RTMONOBJ) $(LIBNETLINK)
	$(CC) $(LDFLAGS) -o $@ $(RTMONOBJ) $(LIBNETLINK) $(LDLIBS$(LDLIBS_$@))

romfs:
	$(ROMFSINST) -e CONFIG_USER_IPROUTE2_IP_IFCFG  /bin/ifcfg
	$(ROMFSINST) -e CONFIG_USER_IPROUTE2_IP_IP     /bin/ip
	$(ROMFSINST) -e CONFIG_USER_IPROUTE2_IP_ROUTEF /bin/routef
	$(ROMFSINST) -e CONFIG_USER_IPROUTE2_IP_ROUTEL /bin/routel
	$(ROMFSINST) -e CONFIG_USER_IPROUTE2_IP_RTMON  /bin/rtmon
	$(ROMFSINST) -e CONFIG_USER_IPROUTE2_IP_RTPR   /bin/rtpr

install: all
	install -m 0755 -s $(TARGETS) $(DESTDIR)$(SBINDIR)
	install -m 0755 $(SCRIPTS) $(DESTDIR)$(SBINDIR)

clean:
	rm -f *.gdb
	rm -f $(ALLOBJ) $(TARGETS)

LDLIBS += -ldl
