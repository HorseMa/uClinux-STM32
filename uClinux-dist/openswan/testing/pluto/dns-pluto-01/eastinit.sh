: ==== start ====
TESTNAME=dns-pluto-01
source /testing/pluto/bin/eastlocal.sh

ipsec setup start
ipsec auto --add dns--westnet-eastnet
ipsec whack --debug-dns 
/testing/pluto/bin/wait-until-pluto-started
