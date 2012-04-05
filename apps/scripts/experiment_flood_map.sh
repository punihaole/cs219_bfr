#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

case "$HOST" in
	n25)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow1 -f
		;;
	n32)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow2 -f
		;;
	n11)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow3 -f
		;;
	n33)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow4 -f
		;;
	n34)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow5 -f
		;;
	n19)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow6 -f
		;;
	n43)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow7 -f
		;;
	n40)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow8 -f
		;;
	n41)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow9 -f
		;;
	n7) 
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow10 -f
		;;
	n5)
		sleep 20
		$MYDIR/../bin/cftp /map/flow1 $MYDIR/../flow1_bfr.txt -f
		;;
	n9)
		sleep 20
		$MYDIR/../bin/cftp /map/flow2 $MYDIR/../flow2_bfr.txt -f
		;;
	n47)
		sleep 20
		$MYDIR/../bin/cftp /map/flow3 $MYDIR/../flow3_bfr.txt -f
		;;
	n22)
		sleep 20
		$MYDIR/../bin/cftp /map/flow4 $MYDIR/../flow4_bfr.txt -f
		;;
	n35)
		sleep 20
		$MYDIR/../bin/cftp /map/flow5 $MYDIR/../flow5_bfr.txt -f
		;;
	n6)
		sleep 20
		$MYDIR/../bin/cftp /map/flow6 $MYDIR/../flow6_bfr.txt -f
		;;
	n10)
		sleep 20
		$MYDIR/../bin/cftp /map/flow7 $MYDIR/../flow7_bfr.txt -f
		;;
	n24)
		sleep 20
		$MYDIR/../bin/cftp /map/flow8 $MYDIR/../flow8_bfr.txt -f
		;;
	n12)
		sleep 20
		$MYDIR/../bin/cftp /map/flow9 $MYDIR/../flow9_bfr.txt -f
		;;
	n36)
		sleep 20
		$MYDIR/../bin/cftp /map/flow10 $MYDIR/../flow10_bfr.txt -f
		;;
	*)
		exit 0
esac
