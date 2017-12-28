#!/bin/sh

TEST_PURPOSE=regress
TEST_TYPE=umlXhost

TESTNAME=x509-pluto-06

XHOST_LIST="NIC NORTH EAST"

REF_CONSOLE_FIXUPS="kern-list-fixups.sed nocr.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS east-prompt-splitline.pl"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS script-only.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS cutout.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS klips-debug-sanitize.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS ipsec-setup-sanitize.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS ipsec-lwdnsq-sanitize.pl"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS pluto-whack-sanitize.sed"

#THREEEIGHT=true

NORTH_PLAY=../../klips/inputs/08-pole-sunrise-ping.pcap
REF_EAST_OUTPUT=east-cleartext.txt

EASTHOST=east
EAST_INIT_SCRIPT=eastinit.sh
EAST_FINAL_SCRIPT=final.sh
REF_EAST_CONSOLE_OUTPUT=east-console.txt
REF26_EAST_CONSOLE_OUTPUT=east-console.txt

NORTHHOST=north
NORTH_INIT_SCRIPT=northinit.sh
NORTH_RUN_SCRIPT=northrun.sh
NORTH_FINAL_SCRIPT=final.sh
REF_NORTH_CONSOLE_OUTPUT=north-console.txt
REF26_NORTH_CONSOLE_OUTPUT=north-console.txt

NICHOST=nic

NETJIG_EXTRA=../x509-pluto-01/debugpublic.txt
