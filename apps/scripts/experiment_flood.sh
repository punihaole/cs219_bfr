#!/bin/sh

MYDIR=$(dirname $0)

#$MYDIR/experiment_flood_response_1.sh
#$MYDIR/experiment_flood_response.sh
#$MYDIR/experiment_stream.sh -f
#$MYDIR/experiment_flood_map_1.sh
#$MYDIR/experiment_flood_popular.sh

HOST=$(hostname)
MYDIR=$(dirname $0)

case "$HOST" in
	n1)
		$MYDIR/../bin/cftps $MYDIR/../data/map.jpg /unit1/file/map -f
		;;
	n10)
		sleep 3
		#$MYDIR/../bin/cftp /unit1/file/map $MYDIR/../data/map_copy.jpg  -f
		;;
	*)
		exit 0
esac
