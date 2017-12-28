TCOBJ= tc.o tc_qdisc.o tc_class.o tc_filter.o tc_util.o \
       tc_monitor.o m_police.o m_estimator.o m_action.o \
       m_ematch.o emp_ematch.yacc.o emp_ematch.lex.o

include ../Config

lower=$(shell echo $(1) | tr [:upper:] [:lower:])
upper=$(shell echo $(1) | tr [:lower:] [:upper:])
TEST_CONFIG_VAL=$(patsubst $(3)_%,%,$(filter $(3)_%,$(foreach i,$(1),$(CONFIG_$(2)_$(i))_$(i))))
TEST_CONFIG=$(call TEST_CONFIG_VAL,$(1),$(2),y) $(call TEST_CONFIG_VAL,$(1),$(2),m)

SCH=atm cbq csz dsmark gred hfsc htb ingress prio red sfq tbf wrr
CLS=fw route rsvp tcindex u32

SCH_OPT = $(call TEST_CONFIG,$(call upper,$(SCH)),NET_SCH)
CLS_OPT = $(call TEST_CONFIG,$(call upper,$(CLS)),NET_CLS)

TEST_OBJ=$(foreach i,$(call lower,$(1)),$(2)_$(i).o)
TEST_CFLAGS=$(foreach i,$(1),-DTC_CONFIG_$(2)_$(i))

TCMODULES :=
TCMODULES += q_fifo.o
TCMODULES += q_sfq.o
TCMODULES += q_red.o
TCMODULES += q_prio.o
TCMODULES += q_tbf.o
TCMODULES += q_cbq.o
TCMODULES += q_rr.o
TCMODULES += q_netem.o
TCMODULES += f_rsvp.o
TCMODULES += f_u32.o
TCMODULES += f_route.o
TCMODULES += f_fw.o
TCMODULES += f_basic.o
TCMODULES += q_dsmark.o
TCMODULES += q_gred.o
TCMODULES += f_tcindex.o
TCMODULES += q_ingress.o
TCMODULES += q_hfsc.o
TCMODULES += q_htb.o
TCMODULES += m_gact.o
TCMODULES += m_mirred.o
TCMODULES += m_ipt.o
TCMODULES += m_pedit.o
TCMODULES += p_ip.o
TCMODULES += p_icmp.o
TCMODULES += p_tcp.o
TCMODULES += p_udp.o
TCMODULES += em_nbyte.o
TCMODULES += em_cmp.o
TCMODULES += em_u32.o
TCMODULES += em_meta.o

TCOBJ += $(TCMODULES)

TCLIB := tc_core.o
TCLIB += tc_red.o
TCLIB += tc_cbq.o
TCLIB += tc_estimator.o

CFLAGS += -DCONFIG_GACT -DCONFIG_GACT_PROB

TCSO :=
ifeq ($(TC_CONFIG_ATM),y)
  TCSO += q_atm.so
endif

ifdef no_defined
LDLIBS += -L. -ltc -lm -ldl

LDFLAGS += -Wl,-export-dynamic
else
LDLIBS := -L. -ltc -lm -ldl $(LDLIBS)
#LDFLAGS += -Wl,-export-dynamic
endif

YACC := bison
LEX := flex

%.so: %.c
	$(CC) $(CFLAGS) -shared -fpic $< -o $@


all: libtc.a tc $(TCSO)

tc: $(TCOBJ) $(LIBNETLINK) $(TCLIB)
	$(CC) $(LDFLAGS) -o $@ $(TCOBJ) $(LIBNETLINK) $(TCLIB) $(LDLIBS$(LDLIBS_$@))

tc.o: qdisc.c filter.c

qdisc.c: $(KERNEL_INCLUDE)/../.config Makefile
	@printf "$(foreach i,$(call lower,pfifo bfifo $(SCH_OPT)),extern struct qdisc_util $(i)_qdisc_util;\n)" > qdisc.c
	@printf "static struct qdisc_util *qdisc[] = {\n" >> qdisc.c
	@printf "$(foreach i,$(call lower,pfifo bfifo $(SCH_OPT)),\t&$(i)_qdisc_util,\n)" >> qdisc.c
	@printf "\tNULL\n};\n" >> qdisc.c

filter.c: $(KERNEL_INCLUDE)/../.config Makefile
	@printf "$(foreach i,$(call lower,$(CLS_OPT)),extern struct filter_util $(i)_filter_util;\n)" > filter.c
	@printf "static struct filter_util *filter[] = {\n" >> filter.c
	@printf "$(foreach i,$(call lower,$(CLS_OPT)),\t&$(i)_filter_util,\n)" >> filter.c
	@printf "\tNULL\n};\n" >> filter.c

romfs:
	$(ROMFSINST) -e CONFIG_USER_IPROUTE2_TC_TC /bin/tc

libtc.a: $(TCLIB)
	$(AR) rcs $@ $(TCLIB)

install: all
	mkdir -p $(DESTDIR)/usr/lib/tc
	install -m 0755 -s tc $(DESTDIR)$(SBINDIR)
	for i in $(TCSO); \
	do install -m 755 -s $$i $(DESTDIR)/usr/lib/tc; \
	done

clean:
	rm -f *.gdb
	rm -f $(TCOBJ) $(TCLIB) libtc.a tc *.so emp_ematch.yacc.h; \
	rm -f emp_ematch.yacc.output

q_atm.so: q_atm.c
	$(CC) $(CFLAGS) -shared -fpic -o q_atm.so q_atm.c -latm

%.yacc.c: %.y
	$(YACC) $(YACCFLAGS) -o $@ $<

%.lex.c: %.l
	$(LEX) $(LEXFLAGS) -o$@ $<
