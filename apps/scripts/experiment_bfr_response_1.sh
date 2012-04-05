#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)
CONTENT='TEST STRING'


case "$HOST" in
	n10)
		$MYDIR/../bin/pchunk /flow1/string "$CONTENT"
		;;
	n1)
		sleep 20
		$MYDIR/../bin/gchunk /flow1/string > $MYDIR/../flow1_bfr.txt
		;;
	*)
		exit 0
esac
