#!/bin/sh
: ==== start ====
TESTNAME=nat-pluto-01
source /testing/pluto/bin/eastlocal.sh

arp -s 192.0.2.1 10:00:00:dc:bc:01

ipsec setup start
/testing/pluto/bin/wait-until-pluto-started
ipsec auto --add northnet--eastnet-nat
: ==== cut ====
ipsec klipsdebug --set rcv
ipsec klipsdebug --set verbose
: ==== tuc ====

echo done


