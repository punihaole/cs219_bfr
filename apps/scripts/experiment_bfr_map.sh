#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

case "$HOST" in
	n25)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow1
		;;
	n32)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow2
		;;
	n11)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow3
		;;
	n33)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow4
		;;
	n34)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow5
		;;
	n19)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow6
		;;
	n43)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow7
		;;
	n40)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow8
		;;
	n41)
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow9
		;;
	n7) 
		$MYDIR/../bin/cftps $MYDIR/../../experiments/map.jpg /map/flow10
		;;
	n5)
		sleep 20
		$MYDIR/../bin/cftp /flow1/string $MYDIR/../flow1_bfr.txt
		;;
	n9)
		sleep 20
		$MYDIR/../bin/cftp /flow2/string $MYDIR/../flow2_bfr.txt
		;;
	n47)
		sleep 20
		$MYDIR/../bin/cftp /flow3/string $MYDIR/../flow3_bfr.txt
		;;
	n22)
		sleep 20
		$MYDIR/../bin/cftp /flow4/string $MYDIR/../flow4_bfr.txt
		;;
	n35)
		sleep 20
		$MYDIR/../bin/cftp /flow5/string $MYDIR/../flow5_bfr.txt
		;;
	n6)
		sleep 20
		$MYDIR/../bin/cftp /flow6/string $MYDIR/../flow6_bfr.txt
		;;
	n10)
		sleep 20
		$MYDIR/../bin/cftp /flow7/string $MYDIR/../flow7_bfr.txt
		;;
	n24)
		sleep 20
		$MYDIR/../bin/cftp /flow8/string $MYDIR/../flow8_bfr.txt
		;;
	n12)
		sleep 20
		$MYDIR/../bin/cftp /flow9/string $MYDIR/../flow9_bfr.txt
		;;
	n36)
		sleep 20
		$MYDIR/../bin/cftp /flow10/string $MYDIR/../flow10_bfr.txt
		;;
	*)
		exit 0
esac
