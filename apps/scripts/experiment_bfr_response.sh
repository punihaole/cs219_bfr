#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)
CONTENT='TEST STRING'

sleep 10

case "$HOST" in
	n25)
		$MYDIR/../bin/pchunk /flow1/string "$CONTENT"
		;;
	n32)
		$MYDIR/../bin/pchunk /flow2/string "$CONTENT"
		;;
	n11)
		$MYDIR/../bin/pchunk /flow3/string "$CONTENT"
		;;
	n33)
		$MYDIR/../bin/pchunk /flow4/string "$CONTENT"
		;;
	n34)
		$MYDIR/../bin/pchunk /flow5/string "$CONTENT"
		;;
	n19)
		$MYDIR/../bin/pchunk /flow6/string "$CONTENT"
		;;
	n43)
		$MYDIR/../bin/pchunk /flow7/string "$CONTENT"
		;;
	n40)
		$MYDIR/../bin/pchunk /flow8/string "$CONTENT"
		;;
	n41)
		$MYDIR/../bin/pchunk /flow9/string "$CONTENT"
		;;
	n7) 
		$MYDIR/../bin/pchunk /flow10/string "$CONTENT"
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
