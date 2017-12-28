#!/bin/sh

: ==== start ====

# get to the 192.0.1.0/24 network via 192.1.3.45
route delete -net 192.0.1.0 netmask 255.255.255.0
route add -net 192.0.1.0 netmask 255.255.255.0 gw 192.1.4.45

# also make sure that the sunrise network is in fact behind .24, because
# we moved things so that we could get the right OE stuff.
route delete -net 192.0.2.0 netmask 255.255.255.0
route add -net 192.0.2.0 netmask 255.255.255.0 gw 192.1.2.24

# now start named
named

# and services
inetd
