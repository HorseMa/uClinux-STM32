: ==== start ====
TESTNAME=ikev2-01-fallback-ikev1
source /testing/pluto/bin/eastlocal.sh

ipsec setup start
ipsec auto --add  westnet--eastnet-ikev2
ipsec whack --debug-control --debug-controlmore --debug-crypt
/testing/pluto/bin/wait-until-pluto-started
