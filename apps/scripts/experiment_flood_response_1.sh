#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)
CONTENT='TEST STRING'


case "$HOST" in
	n10)
		$MYDIR/../bin/pchunk /flow1/string "$CONTENT" -f
		;;
	n1)
		sleep 5
		$MYDIR/../bin/gchunk /flow1/string > $MYDIR/../flow1_flood.txt -f
		;;
	*)
		exit 0
esac
