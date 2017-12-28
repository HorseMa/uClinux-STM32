#!/bin/sh

# we can transmit in the clear
ping -q -c 8 -n 192.1.2.23

# bring up the tunnel
ipsec auto --up west-east

# use the tunnel
#ping -q -c 8 -n 192.1.2.23

# show the tunnel!
echo "Tunnel should be up"
ipsec eroute

# Let R_U_THERE packets flow
sleep 15

echo "Setting up block"
iptables -I INPUT -s 192.1.2.23/32 -d 0/0 -j DROP
iptables -I OUTPUT -d 192.1.2.23/32 -s 0/0 -j DROP
sleep 10
ipsec eroute
sleep 10
ipsec eroute
sleep 10

# DPD should have triggered now
echo "Tunnel should be down on West, not East"
ipsec eroute

# Remove the Blockage
echo "Removing block"
iptables -D INPUT -s 192.1.2.23/32 -d 0/0 -j DROP
iptables -D OUTPUT -d 192.1.2.23/32 -s 0/0 -j DROP
ping -q -c 8 -n 192.1.2.23

# Tunnel should be back up now
echo "Tunnel should be up"
ipsec eroute
echo end
