#!/bin/sh

# this script is used by "east" UMLs that want to have per-test
# configuration files, and will be hitting sunrise-oe to test with.

TESTING=${TESTING-/testing}

if [ "$EAST_USERLAND" == "strongswan" ]
then
        # setup strongswan
        mkdir -p /tmp/strongswan/etc/ipsec.d/certs /tmp/strongswan/etc/ipsec.d/cacerts /tmp/strongswan/etc/ipsec.d/aacerts /tmp/strongswan/etc/ipsec.d/ocspcerts /tmp/strongswan/etc/ipsec.d/crls
        cp /testing/pluto/$TESTNAME/east.conf /tmp/strongswan/etc/ipsec.conf
        cp /testing/pluto/$TESTNAME/east.secrets /tmp/strongswan/etc/ipsec.secrets
        chmod 600 /tmp/strongswan/etc/ipsec.secrets
        touch /tmp/strongswan/etc/ipsec.secrets

elif [ "$WEST_USERLAND" == "racoon2" ]
then
        # setup racoon
        # note: tests do this manual - needs to be merged in
	echo "racoon2 not yet merged into local scripts"
else
	# setup openswan
	mkdir -p /tmp/$TESTNAME
	mkdir -p /tmp/$TESTNAME/ipsec.d/cacerts
	mkdir -p /tmp/$TESTNAME/ipsec.d/crls
	mkdir -p /tmp/$TESTNAME/ipsec.d/certs
	mkdir -p /tmp/$TESTNAME/ipsec.d/private

	cp ${TESTING}/pluto/$TESTNAME/east.conf /tmp/$TESTNAME/ipsec.conf
	cp /etc/ipsec.secrets                    /tmp/$TESTNAME
	if [ -f ${TESTING}/pluto/$TESTNAME/east.secrets ] 
	then
    		cat ${TESTING}/pluto/$TESTNAME/east.secrets >>/tmp/$TESTNAME/ipsec.secrets
	fi

	if [ -f ${TESTING}/pluto/$TESTNAME/east.tpm.tcl ] 
	then
    		cp ${TESTING}/pluto/$TESTNAME/east.tpm.tcl /tmp/$TESTNAME/ipsec.d/tpm.tcl
	fi

	mkdir -p /tmp/$TESTNAME/ipsec.d/policies
	cp -r /etc/ipsec.d/*          /tmp/$TESTNAME/ipsec.d

	IPSEC_CONFS=/tmp/$TESTNAME export IPSEC_CONFS
fi
