: ==== start ====
TESTNAME=dpd-02

source /testing/pluto/bin/eastlocal.sh

ipsec setup start

ipsec auto --add west-east

