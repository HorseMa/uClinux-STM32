: ==== start ====
TESTNAME=algo-pluto-08
source /testing/pluto/bin/eastlocal.sh

ipsec setup start
ipsec auto --add westnet-eastnet-esp-null-alg
/testing/pluto/basic-pluto-01/eroutewait.sh trap
