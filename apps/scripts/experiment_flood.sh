#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

case "$HOST" in
	n29)
		$MYDIR/put_map_1.sh -f
		;;
	n32)
		$MYDIR/put_map_2.sh -f
		;;
	n11)
		$MYDIR/put_map_3.sh -f
		;;
	n33)
		$MYDIR/put_map_4.sh -f
		;;
	n9)
		sleep 5
		$MYDIR/get_map_1.sh -f
		;;
	n5)
		sleep 5
		$MYDIR/get_map_2.sh -f
		;;
#	n47)
#		sleep 5
#		$MYDIR/get_map_3.sh -f
#		;;
#	n22)
#		sleep 5
#		$MYDIR/get_map_4.sh -f
#		;;
	*)
		exit 0
esac
