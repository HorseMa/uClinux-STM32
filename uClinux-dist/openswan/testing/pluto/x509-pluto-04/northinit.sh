#!/bin/sh

: ==== start ====

TESTNAME=x509-pluto-04

ipsec setup start
/testing/pluto/bin/wait-until-pluto-started
ipsec auto --add north-east-x509-pluto-02
echo done
