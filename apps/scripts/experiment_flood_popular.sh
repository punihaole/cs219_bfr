#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

#producer: 25
#consumers: 5  9  47 22 35 6  10 24 12 36

case "$HOST" in
	n25)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /popular/data - -f
		;;
	n5)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow1_flood.txt -f
		;;
	n9)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow2_flood.txt -f
		;;
	n47)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow3_flood.txt -f
		;;
	n22)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow4_flood.txt -f
		;;
	n35)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow5_flood.txt -f
		;;
	n6)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow6_flood.txt -f
		;;
	n10)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow7_flood.txt -f
		;;
	n24)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow8_flood.txt -f
		;;
	n12)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow9_flood.txt -f
		;;
	n36)
		sleep 20
		$MYDIR/../bin/gchunk /popular/data > $MYDIR/../flow10_flood.txt -f
		;;
	*)
		exit 0
esac
