: ==== start ====
TESTNAME=ikev2-algo-01-modp2048-initiator
source /testing/pluto/bin/westlocal.sh

# confirm that the network is alive
ping -n -c 1 192.0.2.254
# make sure that clear text does not get through
iptables -A INPUT -i eth1 -s 192.0.2.0/24 -j DROP
# confirm with a ping to east-in
ping -n -c 1 192.0.2.254

ipsec setup start
ipsec whack --whackrecord /var/tmp/ikev2.record
ipsec auto --add westnet--eastnet-ikev2
ipsec auto --status
/testing/pluto/bin/wait-until-pluto-started

echo done

