#!/bin/sh

#test script that measures the response time of downloading a short string

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

#producers: 25 32 11 33 34 19 43 40 41 7
#consumers: 5  9  47 22 35 6  10 24 12 36

case "$HOST" in
	n25)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow1/string -
		;;
	n32)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow2/string -
		;;
	n11)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow3/string -
		;;
	n33)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow4/string -
		;;
	n34)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow5/string -
		;;
	n19)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow6/string -
		;;
	n43)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow7/string -
		;;
	n40)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow8/string -
		;;
	n41)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow9/string -
		;;
	n7) 
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow10/string -
		;;
	n5)
		sleep 20
		$MYDIR/../bin/gchunk /flow1/string > $MYDIR/../flow1_bfr.txt
		;;
	n9)
		sleep 20
		$MYDIR/../bin/gchunk /flow2/string > $MYDIR/../flow2_bfr.txt
		;;
	n47)
		sleep 20
		$MYDIR/../bin/gchunk /flow3/string > $MYDIR/../flow3_bfr.txt
		;;
	n22)
		sleep 20
		$MYDIR/../bin/gchunk /flow4/string > $MYDIR/../flow4_bfr.txt
		;;
	n35)
		sleep 20
		$MYDIR/../bin/gchunk /flow5/string > $MYDIR/../flow5_bfr.txt
		;;
	n6)
		sleep 20
		$MYDIR/../bin/gchunk /flow6/string > $MYDIR/../flow6_bfr.txt
		;;
	n10)
		sleep 20
		$MYDIR/../bin/gchunk /flow7/string > $MYDIR/../flow7_bfr.txt
		;;
	n24)
		sleep 20
		$MYDIR/../bin/gchunk /flow8/string > $MYDIR/../flow8_bfr.txt
		;;
	n12)
		sleep 20
		$MYDIR/../bin/gchunk /flow9/string > $MYDIR/../flow9_bfr.txt
		;;
	n36)
		sleep 20
		$MYDIR/../bin/gchunk /flow10/string > $MYDIR/../flow10_bfr.txt
		;;
	*)
		exit 0
esac
