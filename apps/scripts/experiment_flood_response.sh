#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)
CONTENT='TEST STRING'

sleep 10

case "$HOST" in
	n25)
		$MYDIR/../bin/pchunk /flow1/string "$CONTENT" -f
		;;
	n32)
		$MYDIR/../bin/pchunk /flow2/string "$CONTENT" -f
		;;
	n11)
		$MYDIR/../bin/pchunk /flow3/string "$CONTENT" -f
		;;
	n33)
		$MYDIR/../bin/pchunk /flow4/string "$CONTENT" -f
		;;
	n34)
		$MYDIR/../bin/pchunk /flow5/string "$CONTENT" -f
		;;
	n19)
		$MYDIR/../bin/pchunk /flow6/string "$CONTENT" -f
		;;
	n43)
		$MYDIR/../bin/pchunk /flow7/string "$CONTENT" -f
		;;
	n40)
		$MYDIR/../bin/pchunk /flow8/string "$CONTENT" -f
		;;
	n41)
		$MYDIR/../bin/pchunk /flow9/string "$CONTENT" -f
		;;
	n7) 
		$MYDIR/../bin/pchunk /flow10/string "$CONTENT" -f
		;;
	n5)
		sleep 20
		$MYDIR/../bin/gchunk /flow1/string > $MYDIR/../flow1_flood.txt -f
		;;
	n9)
		sleep 20
		$MYDIR/../bin/gchunk /flow2/string > $MYDIR/../flow2_flood.txt -f
		;;
	n47)
		sleep 20
		$MYDIR/../bin/gchunk /flow3/string > $MYDIR/../flow3_flood.txt -f
		;;
	n22)
		sleep 20
		$MYDIR/../bin/gchunk /flow4/string > $MYDIR/../flow4_flood.txt -f
		;;
	n35)
		sleep 20
		$MYDIR/../bin/gchunk /flow5/string > $MYDIR/../flow5_flood.txt -f
		;;
	n6)
		sleep 20
		$MYDIR/../bin/gchunk /flow6/string > $MYDIR/../flow6_flood.txt -f
		;;
	n10)
		sleep 20
		$MYDIR/../bin/gchunk /flow7/string > $MYDIR/../flow7_flood.txt -f
		;;
	n24)
		sleep 20
		$MYDIR/../bin/gchunk /flow8/string > $MYDIR/../flow8_flood.txt -f
		;;
	n12)
		sleep 20
		$MYDIR/../bin/gchunk /flow9/string > $MYDIR/../flow9_flood.txt -f
		;;
	n36)
		sleep 20
		$MYDIR/../bin/gchunk /flow10/string > $MYDIR/../flow10_flood.txt -f
		;;
	*)
		exit 0
esac
