export TZ=UTC

(../parentR1 ikev2.record westnet--eastnet-ikev2 parentI1.pcap
    echo TCPDUMP output
    tcpdump -v -v -s 1600 -n -r parentR1.pcap) 2>&1 | sed -f sanity.sed





