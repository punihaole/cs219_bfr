#!/bin/sh

MYDIR=$(dirname $0)

#$MYDIR/experiment_bfr_response_1.sh
$MYDIR/experiment_bfr_response.sh
#$MYDIR/experiment_stream.sh
#$MYDIR/experiment_bfr_map_1.sh
#$MYDIR/experiment_bfr_popular.sh

HOST=$(hostname)
MYDIR=$(dirname $0)

case "$HOST" in
	n1)
		$MYDIR/../bin/cftps $MYDIR/../data/map.jpg /unit1/file/map
		;;
	n2)
		sleep 3
		#$MYDIR/../bin/cftp /unit1/file/map $MYDIR/../data/map_copy.jpg
		;;
	*)
		exit 0
esac
