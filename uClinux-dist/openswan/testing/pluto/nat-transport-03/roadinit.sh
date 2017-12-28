: ==== start ====
TESTNAME=nat-transport-03
source /testing/pluto/bin/roadlocal.sh

iptables -F INPUT
iptables -F OUTPUT

# confirm that the network is alive
ping -n -c 4 192.0.2.254

# make sure that clear text does not get through
iptables -A INPUT -i eth0 -s 192.1.2.23 -p tcp --sport 3 -j REJECT
iptables -A OUTPUT -o eth0 -d 192.1.2.23 -p tcp --dport 3 -j REJECT

ipsec setup start
ipsec auto --add road--east-port3
ipsec auto --add road--east-pass
ipsec whack --debug-control --debug-controlmore --debug-parsing --debug-crypt
/testing/pluto/bin/wait-until-pluto-started

echo done

