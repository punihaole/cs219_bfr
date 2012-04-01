#!/bin/bash

STAT_DIR=~/stat
OUTPUT_DIR=sums
NPROC=0
for file in `ls $STAT_DIR/*.stat`; do
	outfile=${file/%.stat/_summary.txt}
	outfile="${outfile##*/}"
	./parse_stats.pl $file 2>/dev/null 1>$OUTPUT_DIR/$outfile &
	NPROC=$(($NPROC+1))
	if [ "$NPROC" -ge 8 ]; then
		wait
		NPROC=0
	fi
done

wait
	
