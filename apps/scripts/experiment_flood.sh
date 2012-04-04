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
	n5)
		sleep 20
		$MYDIR/../bin/gchunk /flow1/string > $MYDIR/../flow1_flood.txt -f
		;;
#	n9)
#		sleep 20
#		$MYDIR/../bin/gchunk /flow2/string > $MYDIR/../flow2_flood.txt -f
#		;;
#	n47)
#		sleep 20
#		$MYDIR/../bin/gchunk /flow3/string > $MYDIR/../flow3_flood.txt -f
#		;;
#	n22)
#		sleep 20
#		$MYDIR/../bin/gchunk /flow4/string > $MYDIR/../flow4_flood.txt -f
#		;;
	*)
		exit 0
esac
