#ipsec setup stop
#umount /var/tmp; mount /var/tmp
#umount /usr/local; mount /usr/local
: ==== start ====
TESTNAME=psk-pluto-04
source /testing/pluto/bin/eastlocal.sh

ipsec setup start
ipsec auto --add road-east-psk
