#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)
CONTENT='TEST STRING'

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
	n5)
		sleep 20
		$MYDIR/../bin/gchunk /flow1/string > $MYDIR/../flow1_bfr.txt
		;;
#	n9)
#		sleep 20
#		$MYDIR/../bin/gchunk /flow2/string > $MYDIR/../flow2_bfr.txt
#		;;
#	n47)
#		sleep 20
#		$MYDIR/../bin/gchunk /flow3/string > $MYDIR/../flow3_bfr.txt
#		;;
#	n22)
#		sleep 20
#		$MYDIR/../bin/gchunk /flow4/string > $MYDIR/../flow4_bfr.txt
#		;;
	*)
		exit 0
esac
