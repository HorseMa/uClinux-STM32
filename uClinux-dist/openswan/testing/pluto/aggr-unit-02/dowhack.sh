#!/bin/sh

. CONFIG

both --name isakmp-aggr2-psk --psk --aggrmode --ike=3des-sha1 $EASTHOST $TO $WESTHOST $TIMES2 ;
me --name isakmp-aggr2-psk --initiate 

$DOWHACK shutdown 

if [ -f pluto/west/core ];
then
	echo CORE west
	echo CORE west
	echo CORE west
fi

if [ -f pluto/east/core ];
then
        echo CORE east
	echo CORE east
	echo CORE east
fi

