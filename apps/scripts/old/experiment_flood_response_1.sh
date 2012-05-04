#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)


case "$HOST" in
	n10)
		dd if=/dev/urandom bs=1000 count=1 2>/dev/null | $MYDIR/../bin/pchunk /flow1/string - -f
		;;
	n1)
		sleep 5
		$MYDIR/../bin/gchunk /flow1/string > $MYDIR/../flow1_flood.txt -f
		;;
	*)
		exit 0
esac
