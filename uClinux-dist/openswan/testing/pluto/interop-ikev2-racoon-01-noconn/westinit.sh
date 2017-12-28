: ==== start ====
TESTNAME=interop-ikev2-racoon-01-noconn
export PLUTO_EVENT_RETRANSMIT_DELAY=3
export PLUTO_MAXIMUM_RETRANSMISSIONS_INITIAL=4
source /testing/pluto/bin/westnlocal.sh

ipsec setup start
ipsec whack --whackrecord /var/tmp/ikev2.record
ipsec auto --add westnet--eastnet-ikev2
ipsec auto --status
ipsec whack --debug-control --debug-controlmore --debug-parsing --debug-crypt
/testing/pluto/bin/wait-until-pluto-started

echo done

