#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 5

case "$HOST" in
	n10)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow1
		;;
	n1)
		sleep 5
		$MYDIR/../bin/cftp /map/flow1 $MYDIR/../flow1_bfr.jpg > $MYDIR/../flow1_bfr.out
		;;
	*)
		exit 0
esac
