#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

#producers: 25 32 11 33 34 19 43 40 41 7
#consumers: 5  9  47 22 35 6  10 24 12 36

case "$HOST" in
	n25)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow1/string - -f
		;;
	n32)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow2/string - -f
		;;
	n11)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow3/string - -f
		;;
	n33)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow4/string - -f
		;;
	n34)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow5/string - -f
		;;
	n19)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow6/string - -f
		;;
	n43)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow7/string - -f
		;;
	n40)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow8/string - -f
		;;
	n41)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow9/string - -f
		;;
	n7) 
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow10/string - -f
		;;
	n5)
		sleep 20
		$MYDIR/../bin/gchunk /flow1/string -f > $MYDIR/../flow1_flood.txt
		;;
	n9)
		sleep 20
		$MYDIR/../bin/gchunk /flow2/string -f > $MYDIR/../flow2_flood.txt
		;;
	n47)
		sleep 20
		$MYDIR/../bin/gchunk /flow3/string -f > $MYDIR/../flow3_flood.txt
		;;
	n22)
		sleep 20
		$MYDIR/../bin/gchunk /flow4/string -f > $MYDIR/../flow4_flood.txt
		;;
	n35)
		sleep 20
		$MYDIR/../bin/gchunk /flow5/string -f > $MYDIR/../flow5_flood.txt
		;;
	n6)
		sleep 20
		$MYDIR/../bin/gchunk /flow6/string -f > $MYDIR/../flow6_flood.txt
		;;
	n10)
		sleep 20
		$MYDIR/../bin/gchunk /flow7/string -f > $MYDIR/../flow7_flood.txt
		;;
	n24)
		sleep 20
		$MYDIR/../bin/gchunk /flow8/string -f > $MYDIR/../flow8_flood.txt
		;;
	n12)
		sleep 20
		$MYDIR/../bin/gchunk /flow9/string -f > $MYDIR/../flow9_flood.txt
		;;
	n36)
		sleep 20
		$MYDIR/../bin/gchunk /flow10/string -f > $MYDIR/../flow10_flood.txt
		;;
	*)
		exit 0
esac
