#!/bin/sh

ipsec setup start

/testing/pluto/whack-status-01/whack_load.sh

ipsec auto --status
