export TZ=UTC

(../parentR2duplicate ../lib-parentR1/ikev2.record westnet--eastnet-ikev2 ../lib-parentR2/parentI2.pcap 2>&1 | tee secrets.raw
    grep '^| ikev2 [IR]' secrets.raw | cut -c3- >ike-secrets.txt
    echo TCPDUMP output
    tcpdump -v -v -s 1600 -n -E 'file ike-secrets.txt' -r parentR2.pcap) 2>&1 | sed -f ../lib-parentR2/sanity.sed

