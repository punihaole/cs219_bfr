#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

case "$HOST" in
	n10)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow1 -f
		;;
	n1)
		sleep 20
		$MYDIR/../bin/cftp /map/flow1 $MYDIR/../flow1_flood.txt -f > $MYDIR/../flow1_flood.out
		;;
	*)
		exit 0
esac
