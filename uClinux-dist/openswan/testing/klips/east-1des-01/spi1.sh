#!/bin/sh
: ==== start ====
TZ=GMT export TZ

insmod /testing/packaging/modtest-cryptoapi-02/OUTPUT/module/ipsec.ko

ipsec spi --clear
ipsec eroute --clear

enckey=0x4043434545464646
authkey=0x87658765876587658765876587658765

ipsec spi --af inet --edst 192.1.2.45 --spi 0x78453412 --proto esp --src 192.1.2.23 --esp des-md5-96 --enckey $enckey --authkey $authkey

ipsec spi --af inet --edst 192.1.2.45 --spi 0x78453412 --proto tun --src 192.1.2.23 --dst 192.1.2.45 --ip4

ipsec spigrp inet 192.1.2.45 0x78453412 tun inet 192.1.2.45 0x78453412 esp 

ipsec eroute --add --eraf inet --src 192.0.2.0/24 --dst 192.0.1.0/24 --said tun0x78453412@192.1.2.45

ipsec tncfg --attach --virtual ipsec0 --physical eth1
ifconfig ipsec0 inet 192.1.2.23 netmask 0xffffff00 broadcast 192.1.2.255 up

arp -s 192.1.2.45 10:00:00:64:64:45
arp -s 192.1.2.254 10:00:00:64:64:45

ipsec look

# magic route command
route add -host 192.0.1.1 gw 192.1.2.45 dev ipsec0
: ==== end ====

