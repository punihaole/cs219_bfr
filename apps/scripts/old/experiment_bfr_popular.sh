#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

#producer: 25
#consumers: 5  9  47 22 35 6  10 24 12 36

case "$HOST" in
	n25)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /popular/data -
		;;
	n5)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow1_bfr.txt
		;;
	n9)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow2_bfr.txt
		;;
	n47)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow3_bfr.txt
		;;
	n22)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow4_bfr.txt
		;;
	n35)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow5_bfr.txt
		;;
	n6)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow6_bfr.txt
		;;
	n10)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow7_bfr.txt
		;;
	n24)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow8_bfr.txt
		;;
	n12)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow9_bfr.txt
		;;
	n36)
		sleep 110
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow10_bfr.txt
		;;
	*)
		exit 0
esac
