: ==== start ====
# confirm that the network is alive
ping -n -c 4 192.0.2.254
# make sure that clear text does not get through
iptables -A INPUT -i eth1 -s 192.0.2.0/24 -j DROP
# confirm with a ping to east-in
ping -n -c 4 192.0.2.254

TESTNAME=algo-pluto-09
source /testing/pluto/bin/westlocal.sh

ipsec setup start
ipsec auto --add westnet-eastnet-esp-3des-alg
/testing/pluto/basic-pluto-01/eroutewait.sh trap

echo done

