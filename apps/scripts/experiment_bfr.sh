#!/bin/sh

HOST=$(hostname)
MYDIR=$(dirname $0)

sleep 10

case "$HOST" in
	n29)
		$MYDIR/put_map_1.sh
		;;
	n32)
		$MYDIR/put_map_2.sh
		;;
#	n11)
#		$MYDIR/put_map_3.sh
#		;;
#	n33)
#		$MYDIR/put_map_4.sh
#		;;
	n9)
		sleep 5
		$MYDIR/get_map_1.sh
		;;
	n5)
		sleep 5
		$MYDIR/get_map_2.sh
		;;
#	n47)
#		sleep 5
#		$MYDIR/get_map_3.sh
#		;;
#	n22)
#		sleep 5
#		$MYDIR/get_map_4.sh
#		;;
	*)
		
		exit 0
esac
