#!/bin/sh
: ==== start ====
TZ=GMT export TZ

ipsec spi --clear
ipsec eroute --clear

enckey=0x4043434545464649494a4a4c4c4f4f515152525454575758
authkey=0x87658765876587658765876587658765

ipsec spi --dumpsaref --af inet --edst 192.1.2.45 --spi 0x12345678 --proto esp --src 192.1.2.23 --esp 3des-md5-96 --enckey $enckey --authkey $authkey

ipsec spi --dumpsaref --af inet --edst 192.1.2.45 --spi 0x12345678 --proto tun --src 192.1.2.23 --dst 192.1.2.45 --ip4


