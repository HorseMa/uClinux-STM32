#ipsec setup stop
#umount /var/tmp; mount /var/tmp
#umount /usr/local; mount /usr/local

TESTNAME=psk-pluto-06
source /testing/pluto/bin/eastlocal.sh

ipsec setup start
ipsec auto --add road--eastnet-psk
