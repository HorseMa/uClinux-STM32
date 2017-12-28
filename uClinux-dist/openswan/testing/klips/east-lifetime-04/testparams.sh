#!/bin/sh

TEST_TYPE=klipstest
TEST_PURPOSE=regress
TESTNAME=east-lifetime-04
TESTHOST=east
EXITONEMPTY=--exitonempty
ARPREPLY=--arpreply

REF_PUB_OUTPUT=spi1-output.txt
PRI_VINPUT=../inputs/01-sunrise-sunset-ping.pcap
REF_CONSOLE_OUTPUT=test-04-console.txt
REF_CONSOLE_FIXUPS="kern-list-fixups.sed nocr.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS klips-spi-sanitize.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS klips-debug-sanitize.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS ipsec-look-sanitize.sed"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS east-prompt-splitline.pl"
REF_CONSOLE_FIXUPS="$REF_CONSOLE_FIXUPS pfkey-time-cleanup.sed"
TCPDUMPFLAGS="-n -E 3des-cbc-hmac96:0x4043434545464649494a4a4c4c4f4f515152525454575758"
INIT_SCRIPT=test01.sh
FINAL_SCRIPT=../east-trap-01/pfkeyhalt.sh




