: ==== start ====
TESTNAME=dpd-04
source /testing/pluto/bin/eastlocal.sh

ipsec setup start

/testing/pluto/bin/wait-until-policy-loaded

ipsec auto --add west-east
ipsec auto --add west-eastnet
ipsec auto --add westnet-east




